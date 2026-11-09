# relaxation-reminder 技术方案

- 版本：v0.1（草案）
- 日期：2026-11-06
- 范围：Win11 实现方案；macOS、iOS 给出设计但不实现
- 基准环境：Windows 11 Pro 26200、x64、物理内存 63.6 GB、VS2019 Community（MSVC 14.29 / v142）、Windows SDK 10.0.19041

## 1. 结论摘要

1. Win11 端采用 **C++17 + 原生 Win32 API**，单进程、无第三方库、静态链接 CRT（`/MT`）；核心程序为单个 exe（约 250 KB）。默认由 AI 智能体按 README 提示词自动安装；手动备用方式为每用户 Inno Setup 安装包（约 2.2 MB）。不提供便携版。
2. 计时全部基于单调时钟 `GetTickCount64`；键鼠空闲检测使用 `GetLastInputInfo`，轮询周期 50 ms，不装全局钩子；空闲阈值由 `idle_threshold_seconds` 配置，默认 10 秒。
3. 提醒窗口从鼠标位置出现，面积按 `A(t)=A0·e^{kt}` 增长，60 秒达到所在显示器工作区面积的 30% 后停止。
4. 状态机、扩大曲线、配置校验放在可移植的 `core/` 中，macOS 用 ObjC++ 复用；iOS 因系统限制用 Swift 重写精简逻辑。
5. 1% 内存指标在本机为约 651 MB，方案自定目标为 **私有工作集 ≤20 MB**，保证在 8 GB 机器（1%≈82 MB）上同样达标。
6. 窗口形象默认由 GDI 绘制；用户可指定 PNG/JPEG/BMP/GIF（含动图），由系统自带 GDI+ 解码，失败自动回退；用户资源不计入 1 MB 发布产物。

## 2. 本轮需求变更确认

| 项目 | 原需求 | 现需求（2026-11-06 确认） |
|---|---|---|
| 窗口位置 | 窗口跟随鼠标 | 从鼠标位置出现，显示期间不逐像素跟随 |
| 窗口尺寸 | 固定大小 | 初始 320×320（按 DPI 缩放），面积指数扩大，60 秒达到工作区面积 30% 后停止 |
| 显示时长 | 显示满 `rest_duration_minutes` 自动消失 | 键鼠无输入满 10 秒才开始休息倒计时；倒计时满才消失 |
| 休息判定 | 无 | 仅系统级键鼠空闲，锁屏/睡眠不额外触发 |
| 休息中断 | 无 | 休息计时暂停；再空闲 10 秒后继续；窗口仍按既定曲线扩大 |
| 窗口关闭 | 无确认 | 点击“关闭”二次确认，提示休息并说明下次提醒时间 |
| 延后操作 | 无确认 | 点击“延后”二次确认，说明延后时长与仅可延后一次 |
| 设置入口 | 无 | 提醒窗口左下角“设置”按钮 / 托盘菜单，参数面板支持一键恢复默认值 |
| 延后按钮 | 重新弹出后不可延后 | 按钮保留并禁用置灰 |
| 参数修改 | 未定义 | 下一个提醒周期生效 |
| 占位资源 | 卡通形象占位图 | 默认程序绘制正方形；支持用户添加 PNG/JPEG/BMP/GIF（含动图），失败回退内置图 |
| 发布形态 | 未定义 | 默认 AI 智能体自动安装；备用每用户安装包（Inno Setup）+ `HKCU\...\Run` 自启；卸载保留配置与形象 |
| 空闲阈值 | 固定 10 秒 | 第 4 个参数 `idle_threshold_seconds`，默认 10 秒，可配置 |
| iOS 体积 | 统一 1 MB | 允许例外，iOS 单列记录 |
| 立即休息 | 未定义 | 托盘菜单“立即休息”，点击立即显示提醒窗口 |

## 3. 技术栈选型

### 3.1 硬约束

- 核心 exe ≤1 MB（不含 OS 自带运行时与系统框架）；**大小限制已放宽**：安装包单独统计，不计入 1 MB 判定（设计目标 ≤3 MB）。
- 常驻空闲内存 ≤系统物理内存 1%，低空闲 CPU。
- 需要置顶无焦点窗口、托盘、注册表自启、多显示器/DPI、全局键鼠空闲检测。

### 3.2 候选对比

| 方案 | 预计产物 | 空闲内存 | 开发成本 | 本机可构建 | 结论 |
|---|---|---|---|---|---|
| C++17 + Win32 + MSVC `/MT` | 120–250 KB | 5–15 MB | 高（窗口/托盘手写） | 是（MSVC v142 + SDK） | **推荐** |
| Rust + windows-rs | 200–500 KB | 5–15 MB | 中 | 否（需安装工具链） | 备选，体积可控但需新增工具链 |
| C# .NET 8 自包含 | 15–80 MB | 30–60 MB | 低 | 否（无 .NET SDK） | 不满足 1 MB |
| C# .NET 8 框架依赖 | 数百 KB | 30–60 MB | 低 | 否 | 需用户另装运行时，不满足“绿色” |
| Electron | 80 MB+ | 100 MB+ | 低 | 否 | 不满足 |
| Tauri | 3–10 MB | 30–80 MB | 低 | 否 | 不满足 1 MB |
| WinUI 3 / Windows App SDK | 数 MB 起 | 30 MB+ | 中 | 否 | 不满足 |

### 3.3 选型结论

- 语言/工具链：**C++17、MSVC v142（VS2019）、Windows SDK 10.0.19041、x64**。
- 链接：`/MT` 静态 CRT，避免 VC++ 运行时分发；`/O2 /Gy /OPT:REF /OPT:ICF`，关闭 RTTI，异常仅在必要处使用。
- 依赖：仅系统库 `user32.lib gdi32.lib gdiplus.lib comdlg32.lib comctl32.lib uxtheme.lib shell32.lib advapi32.lib shcore.lib winmm.lib delayimp.lib`，不引入任何第三方库。GDI+（`gdiplus.dll`）与 winmm 均为 Windows 自带组件；winmm 通过 `/DELAYLOAD` 延迟到首次播放音乐时加载。
- 界面：原生 Win32 窗口 + 标准 `BUTTON` 控件 + GDI 绘制占位方块 + GDI+ 解码用户形象，不引入 UI 框架。
- 体积预算：exe ≤450 KB（硬指标 1 MB）；资源为图标、版本信息与约 94 KB 内置音乐。

### 3.4 代码结构

```text
src/
  core/                    可移植 C++（不依赖 Win32，macOS 复用）
    state_machine.h/.cpp   提醒/休息/延后状态机
    config.h/.cpp          参数校验、默认值、序列化
    growth.h/.cpp          指数扩大曲线
  win32/
    main.cpp               WinMain、单实例、消息循环
    platform.cpp           时钟、空闲检测、显示器、DPI
    reminder_window.cpp    提醒窗口与绘制
    character_image.cpp    自定义图片/动图加载、校验、逐帧绘制
    rest_sound.cpp         休息结束音乐：WAV 校验、内置/用户音乐 waveOut 流式播放与淡出
    settings_dialog.cpp    设置界面
    tray.cpp               托盘图标与菜单
    autostart.cpp          HKCU Run 读写
  resources/
    app.rc / app.manifest / icon.ico
```

首版允许 `core/` 与 `win32/` 同目录编译，但接口边界保持清晰，便于 macOS 移植。

## 4. 总体架构

- **单进程、单线程**：所有窗口、计时器、托盘逻辑都在 UI 线程，不创建后台线程。
- **后台窗口**：创建一个隐藏的顶层窗口（不可见、无任务栏按钮）承载 `WM_TIMER` 与 `WM_POWERBROADCAST`；不使用 message-only window，因为后者收不到广播消息。
- **启动序列**：单实例互斥体 → 读取配置并校验 → 创建托盘与消息窗口 → 自启开启时按当前 exe 路径重写自启项（用户关闭过则不写）→ 进入 `WAIT_INTERVAL`。
- **退出**：仅通过托盘菜单“退出”，退出前保存配置、移除计时器。
- **无控制台、无服务、无驱动**；安装包只负责部署，核心程序可独立运行；以当前用户权限运行（`asInvoker`）。

## 5. 状态机

| 状态 | 含义 | 窗口 | 计时 |
|---|---|---|---|
| `WAIT_INTERVAL` | 等待下一次提醒 | 隐藏 | `interval_remaining` |
| `SNOOZE_WAIT` | 延后等待 | 隐藏 | `snooze_remaining` |
| `SHOW_ACTIVE` | 提醒已弹出，尚未开始休息 | 显示、指数扩大 | 空闲检测 |
| `REST_RUNNING` | 已判定开始休息 | 显示、继续扩大 | `rest_remaining` |
| `REST_PAUSED` | 休息中检测到键鼠输入 | 显示、继续扩大 | 休息计时暂停 |

转移规则：

1. `WAIT_INTERVAL` 到期 → `SHOW_ACTIVE`：窗口在鼠标位置以初始尺寸出现；`snooze_used=false`；扩大计时从 0 开始。
2. `SHOW_ACTIVE` 中键鼠空闲满 `idle_threshold_seconds` → `REST_RUNNING`：启动/继续休息倒计时；扩大计时不中断。
3. `REST_RUNNING` 中检测到键鼠输入 → `REST_PAUSED`：暂停倒计时；扩大计时不中断。
4. `REST_PAUSED` 中键鼠空闲满 `idle_threshold_seconds` → `REST_RUNNING`：从暂停处继续倒计时。
5. `REST_RUNNING` 倒计时归零 → 窗口消失，记为“最终消失” → `WAIT_INTERVAL`（`interval_remaining = reminder_interval_minutes`）。
6. 任意可见状态下手动关闭 → 立即消失，记为“最终消失” → `WAIT_INTERVAL`。
7. `SHOW_ACTIVE` / `REST_RUNNING` / `REST_PAUSED` 中点击延后且 `snooze_used=false` → 窗口消失，`snooze_used=true` → `SNOOZE_WAIT`（`snooze_remaining = snooze_duration_minutes`）。
8. `SNOOZE_WAIT` 到期 → `SHOW_ACTIVE`：窗口回到初始尺寸、在鼠标位置重新出现，延后按钮禁用置灰。
9. `SNOOZE_WAIT` 期间不触发下一次提醒；下一次间隔只在“最终消失”后开始计时。

补充约定：

- 托盘“暂停计时”设置全局冻结标记：`Tick` 只更新时钟基准、不推进任何倒计时与扩大进度，状态保持不变；关闭“暂停计时”后从冻结处继续。暂停期间手动关闭/延后仍可操作。
- “延后”“关闭”的二次确认框由同一 UI 线程的 `MessageBox` 弹出，其模态消息循环仍会投递 `WM_TIMER`。弹出前先置全局冻结标记，返回后恢复原值（若用户此前手动暂停则保持暂停），因此确认框打开期间休息倒计时与窗口扩大都不走表。
- 休息倒计时归零与手动关闭都算“最终消失”，均从该时刻起算 `reminder_interval_minutes`。
- 延后按钮在重新弹出的窗口中保留但置灰（`EnableWindow(FALSE)`）。
- 参数修改只在**下一个提醒周期**生效：等待中改间隔不影响当前等待；显示中改休息时长不影响当前倒计时；改延后时长不影响已开始的延后等待。

## 6. 计时、空闲检测与窗口扩大

### 6.1 单调时钟

- 所有截止时间用 `GetTickCount64()`（毫秒，64 位，跨睡眠继续计数），保存“剩余毫秒”或“截止 tick”，不保存墙上时间。
- 动画与空闲检测共用一个 50 ms `WM_TIMER`；每次 tick 按 `now - last_tick` 推进，不做累加漂移。
- 系统时间调整（`WM_TIMECHANGE`）不影响计时。
- `WM_POWERBROADCAST`：
  - `PBT_APMSUSPEND`：冻结休息倒计时（记录 `rest_remaining` 不再扣减）与扩大进度，窗口状态保持不变；睡眠时间不计入休息时长和扩大时长。
  - `PBT_APMRESUMEAUTOMATIC`：立即执行一次 tick 并重新做空闲判定。若 `WAIT_INTERVAL` / `SNOOZE_WAIT` 已超时，只弹出一次提醒，不补发；若窗口仍在显示且键鼠仍空闲满 `idle_threshold_seconds`，则进入/继续休息倒计时。
- 锁屏不做特殊处理：锁屏期间键鼠无输入，空闲判定自然会进入休息；解锁后若窗口仍在且用户操作，转入 `REST_PAUSED`。

### 6.2 键鼠空闲检测

- 使用 `GetLastInputInfo()` 获取当前会话最后一次键鼠输入的 32 位 tick，计算 `idle_ms = (DWORD)GetTickCount64() - last_input_tick`（32 位无符号差值，自动处理约 49.7 天回绕）。
- 阈值取 `idle_threshold_seconds`（默认 10 秒），50 ms 轮询一次；空闲时间 `>= threshold × 1000 ms` 即判定开始休息。
- 不安装 `WH_KEYBOARD_LL` / `WH_MOUSE_LL` 钩子，不需要管理员权限，不读取输入内容。
- 仅覆盖当前登录会话；RDP 会话内独立判断，符合预期。

### 6.3 指数扩大曲线

- 初始逻辑尺寸 320×320；按 DPI 换算物理初始边长 `w0 = round(320 × dpi / 96)`，初始面积 `A0 = w0²`。
- 上限面积 `A_cap = 0.30 × 工作区宽 × 工作区高`（物理像素，仅鼠标所在显示器）。
- 增长系数 `k = ln(A_cap / A0) / 60`（单位：每秒）；若 `A0 >= A_cap`，不扩大。
- 显示后第 `t` 秒：`A(t) = min(A0 · e^{k·t}, A_cap)`；线性缩放比 `s = sqrt(A(t) / A0)`。
- 窗口保持正方形：`w = h = round(w0 · s)`；同时用工作区宽高做硬限制，避免超出屏幕。
- 以 20 Hz（50 ms）更新窗口尺寸，60 秒约 1200 次 `SetWindowPos`，CPU 开销可忽略。
- 扩大进度只随显示时间推进，**不受键鼠空闲、进入 `REST_RUNNING` 或 `REST_PAUSED` 影响**；达到上限后尺寸保持。只有新的提醒弹出（含延后重弹）才重置为 `A0`。
- 托盘“暂停计时”与“延后/关闭”的二次确认框打开期间，扩大进度冻结；恢复后从冻结处继续。
- 睡眠/休眠期间扩大计时冻结（与休息倒计时一致），唤醒后从冻结处继续。
- 窗口以初始光标位置为中心向外扩大；扩大过程中中心点保持不动，并钳制在工作区内。

### 6.4 窗口定位与焦点

- 弹出时取 `GetCursorPos` → `MonitorFromPoint` → `GetMonitorInfo` 得到工作区，窗口居中于光标并钳制在工作区内。
- 多显示器：只在弹出时选择显示器，显示期间不跨屏跟随；DPI 使用 Per-Monitor V2。
- 窗口样式：`WS_POPUP`，扩展样式 `WS_EX_TOPMOST | WS_EX_TOOLWINDOW`。
- 显示使用 `ShowWindow(hwnd, SW_SHOWNOACTIVATE)`，不调用 `SetForegroundWindow`，不抢占键盘焦点；用户点击按钮时窗口才正常激活。
- 不显示任务栏按钮；窗口始终置顶，但允许被用户手动关闭。

## 7. 界面设计

### 7.1 提醒窗口

- 初始 320×320 逻辑像素，随 DPI 缩放；布局自上而下：
  - 176×176 形象区：默认绘制正方形占位图（圆角矩形 + 简单表情线条，全部用 GDI，无图片资源）；用户设置自定义形象后改为绘制图片/动图，按比例 contain 居中。
  - 标题“{user_name}，休息一下”，`user_name` 默认“昵称”。
  - 状态行，随状态变化：
    - `SHOW_ACTIVE`：“离开键盘和鼠标 {idle_threshold_seconds} 秒即开始休息”
    - `REST_RUNNING`：“休息中 03:12 / 05:00”
    - `REST_PAUSED`：“休息已暂停，离开键鼠 {idle_threshold_seconds} 秒后继续”
    - 全局暂停时（任意状态）：“计时已暂停”
  - 底部按钮：`设置`（左下角）、`延后`、`关闭`（右下角，右对齐）。
- 延后按钮文案固定为“延后”，重新弹出后 `EnableWindow(FALSE)` 并置灰，不使用隐藏方案。
- 点击“延后”弹出二次确认：说明 `snooze_duration_minutes` 分钟后重新提醒、每次提醒仅可延后一次；取消则保持当前提醒。
- 点击“关闭”弹出二次确认：提示离开键鼠休息，说明关闭后本次提醒结束、`reminder_interval_minutes` 分钟后再次提醒；取消则保持当前提醒。
- “设置”按钮始终可用，打开参数面板（见 7.2）；面板独立置顶，不随提醒窗口消失。
- 关闭按钮始终可用；窗口不提供标题栏，避免额外绘制与焦点行为。
- 绘制采用内存 DC 双缓冲：整帧画到后台位图后一次 `BitBlt` 上屏，`WM_ERASEBKGND` 直接返回 1，避免闪烁。
- 状态行文字变化（每秒一次）只 `InvalidateRect` 状态行矩形，不重绘整窗；窗口尺寸变化时才整窗重绘。后台位图按 64 px 对齐缓存，扩大过程中不逐帧重新分配。
- 所有自绘窗口与控件统一使用系统 Microsoft YaHei UI（`CreateFont`，随 DPI 调整），不打包字体。
- 窗口面积扩大时内容按比例缩放有限度：占位图与文字保持在窗口内居中，最小可读字号 9 pt；达到 30% 上限后布局稳定。

### 7.2 参数面板

- 入口：提醒窗口左下角“设置”按钮，或托盘菜单“设置”；单实例，已打开时置前。
- 独立置顶小窗口（`WS_EX_TOPMOST | WS_EX_TOOLWINDOW`），居中于鼠标所在显示器；不阻塞提醒窗口，也不随提醒窗口隐藏。
- 字段与校验：

| 字段 | 标签 | 控件与可选值 | 默认值 |
|---|---|---|---|
| `reminder_interval_minutes` | 提醒间隔 | 拖动条 20/30/40/50/60 | 30 |
| `rest_duration_minutes` | 休息时长 | 拖动条 5/10/15 | 5 |
| `snooze_duration_minutes` | 延后时长 | 拖动条 5/10/15 | 5 |
| `idle_threshold_seconds` | — | 不在面板显示，仅配置文件 | 10 |
| `user_name` | 显示名称 | 文本框 | 昵称 |

- 所见即所得：不设“保存/取消”按钮，拖动条或文本框变化立即写入配置并生效。关闭按钮或右上角 X 只隐藏面板，不回滚已生效的修改。
- 拖动条使用 `msctls_trackbar32`，`TBM_SETRANGE` 的档位索引映射到上表可选值；`WM_HSCROLL` 时立即应用，右侧只显示当前值（如“30 分钟”），不展示默认值或校验提示。
- 配置文件中的历史值若不在档位内，打开面板时就近归入并保存，保证面板显示与配置一致。
- 显示名称：合法输入立即生效（去首尾空格、超长截断到 20 字符）；为空或含控制字符时，配置立即使用“牛马”，失焦后输入框同步显示“牛马”，不显示红色校验提示。
- “恢复默认值”点击后立即把提醒间隔、休息时长、延后时长与显示名称填为默认值（30/5/5、昵称）、即时写入配置并生效，面板保持打开；空闲阈值与开机自启状态不受影响。
- 时间参数按 5 节约定在下一提醒周期生效，`user_name` 立即更新提醒标题。
- 开机自启不在参数面板中，只保留托盘菜单“开机自启”；托盘切换时立即写入/删除 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\RelaxationReminder`（`--no-autostart` 调试模式下跳过注册表同步），勾选态以注册表实际状态为准。参数面板的应用回调不修改自启值，避免面板打开期间覆盖托盘的切换。
- 形象图片的“选择图片/恢复默认形象”保留在托盘菜单（见 7.4），不在参数面板中重复。

### 7.3 托盘

- 常驻托盘图标，提示文本“休息提醒”。
- 右键菜单：`设置`、`立即休息`、`暂停计时`（勾选态）、`选择形象图片`、`恢复默认形象`、`开机自启`（勾选态）、`退出`。`立即休息` 立即弹出提醒窗口（若已在显示则不做处理），并先关闭设置面板和启动提示、解除全局暂停。
- `暂停计时` 切换状态机的全局冻结标记：等待间隔、延后等待、休息倒计时与窗口扩大全部停止推进，托盘提示显示“计时已暂停”；再次点击从冻结处继续。
- 图标内嵌多尺寸 ICO（16/20/24/32/40/48/64/128/256 px，约 29 KB），与 exe/安装包图标一致；资源缺失时回退系统默认图标。
- 左键双击打开设置；右键弹出菜单。

### 7.4 自定义形象接口

- 配置项 `character_image_file` 只保存文件名，空值表示内置方块；文件统一存放于 `%APPDATA%\RelaxationReminder\character\`，避免用户移动原文件后失效。
- 选择流程：`GetOpenFileNameW` 选择文件 → 校验扩展名与上限 → 用 GDI+ 实际解码验证 → 复制为新文件并删除旧文件 → 更新配置并立即重绘。任一步失败都保留原形象，并在设置页提示。
- 格式白名单：PNG、JPEG、BMP、GIF（含动画）。不支持 SVG、WebP；APNG 按静态 PNG 首帧处理。
- 上限：文件 ≤10 MB；单帧 ≤1024×1024；GIF 帧数 ≤500；零尺寸、损坏或超限文件一律拒绝。
- 解码：GDI+ `Image` 从文件加载；GIF 用 `GdipImageGetFrameCount` / `GdipImageSelectActiveFrame` 逐帧选择，读取帧延迟（属性 0x5100），夹取到 20–1000 ms，缺失时按 100 ms。
- 绘制：当前帧按比例 contain 到形象区，居中，使用 `HighQualityBicubic`；带 alpha 的图片混合到窗口背景。
- 动画驱动：复用 50 ms tick 累计帧时间，仅在窗口可见时播放；窗口隐藏后重置到首帧。
- 内存：只保留一个 `Image` 对象与当前解码帧（RGBA 最大 4 MB）；自定义形象场景峰值目标 ≤50 MB，仍低于 8 GB 机器 1% 的 82 MB；默认形象场景维持 ≤20 MB。
- 回退：文件缺失、格式不符、解码失败或超限时静默使用内置方块；提醒弹出时不弹错误框，设置页可显示原因。

### 7.5 启动提示

- 每次启动（含开机自启）在托盘图标就绪后显示一次，4 秒后自动关闭；点击提示可立即关闭。
- 340×108 逻辑像素，居中显示在鼠标所在显示器工作区（垂直约 2/5 处）；置顶、`WS_EX_NOACTIVATE`，不抢键盘焦点、不占任务栏。
- 内容：“{user_name}，休息提醒已启动”、深灰说明“将在 `reminder_interval_minutes` 分钟后提醒你休息。”、浅灰提示“可在托盘图标上右键打开设置。”
- 通过 `--remind-now` 测试强制弹出提醒时不显示启动提示，避免两个窗口重叠。

### 7.6 休息结束音乐

- 状态机新增 `MusicPlaying`：`RestRunning` 倒计时归零且 `music_enabled` 为真时进入该状态，产生 `TickResult::music_start`，窗口保持可见；`music_enabled` 为假时保持旧行为，直接 `final_dismiss`。
- 时长：`music_play_seconds`（30–120，默认 60），在 `Configure()` 中排队，下一个提醒周期生效；`music_enabled` 通过 `SetMusicEnabled()` 立即生效。
- 结束：播放满 `music_play_seconds` 产生 `final_dismiss`；播放期间检测到键鼠输入（`idle_ms < idle_threshold`）即提前进入淡出。
- 淡出：状态机在剩余 ≤ `kMusicFadeMs`（1000 ms）时产生一次 `music_fade_out`，剩余时间截断为 1000 ms，随后 `final_dismiss`；音频引擎自身淡出 800 ms，早于窗口关闭完成。
- 标题：`MusicPlaying` 期间提醒标题为“生产队的驴都不敢这么歇！”，状态栏显示“音乐播放中 MM:SS / MM:SS”；其余状态为“{user_name}，休息一下”。
- 暂停：全局暂停计时冻结音乐倒计时并停止音频，恢复后若仍在 `MusicPlaying` 且未淡出则从当前位置继续播放；二次确认框打开期间同样暂停。
- 延后：`CanSnooze()` 在 `MusicPlaying` 期间返回 false，“延后”按钮置灰。
- 播放：`waveOut` 流式播放（winmm，`/DELAYLOAD:winmm.dll`）。内存 WAV 解析后 4 个 40 ms `WAVEHDR` 循环排队，`WAVE_MAPPER`、`CALLBACK_NULL`，由 50 ms UI tick 中的 `ServiceRestMusic()` 回收并重新填充，避免回调线程。短音乐循环播放以覆盖 30–120 秒。
- 循环间隔：每次播放完一遍后插入 3 秒静音（`kLoopGapMilliseconds = 3000`），即“2 秒音乐、3 秒停顿”交替。静音由 `FillBuffer` 在样本位置越过 `data` 块末尾后输出零值实现，不额外占用内存；对内置音乐与用户 WAV 一视同仁。淡出包络在静音段同样生效，因此停顿中动键鼠也能正常淡出并关闭。
- 淡出实现：软件音量包络（8-bit 无符号 / 16-bit 有符号缩放），对尚未排队的音频生效；`FadeOutRestMusic()` 记录当前样本位置，800 ms 线性降到 0。安全超时（淡出 + 2 s）后 `waveOutReset` / `waveOutUnprepareHeader` / `waveOutClose` 释放设备。
- 停止：`StopRestMusic()` 在手动关闭、延后、暂停计时、`PBT_APMSUSPEND`、程序退出、窗口最终消失时调用。
- 格式白名单：PCM WAV（`format=1`）、8/16 位、1~2 声道、8~48 kHz、`data` 块非空，单文件 ≤5 MB；只接受裸文件名，文件存放于 `%APPDATA%\RelaxationReminder\sound\`。压缩 WAV/MP3/AAC 不支持，避免 N 版编解码器缺失。
- 内置音乐：`src/resources/rest_music.wav`，由 `tools/make_music.py` 纯数学生成（复刻参考提示音前 2 秒：1049 Hz 铃声叠加 2449/4131 Hz 泛音），24 kHz 单声道 16-bit PCM、2.0 秒、约 94 KB，以资源类型 `WAVE`、ID 2 编译进 exe；只含 RIFF/fmt/data 块，无元数据、无第三方版权。
- 音量：不提供单条音量控制，由系统音量决定。
- 设置入口：设置面板增加“音乐时长”拖动条（30–120，步长 10）与“休息结束时播放音乐”复选框；托盘菜单增加勾选项、“选择音乐”“恢复默认音乐”。“恢复默认值”将音乐时长恢复 60 秒，不改动音乐开关与用户文件。

## 8. 配置与持久化

- 路径：`%APPDATA%\RelaxationReminder\config.ini`。
- 格式（INI，UTF-16 LE 带 BOM，便于用 `GetPrivateProfileStringW` 读取中文名称）：

```ini
[general]
reminder_interval_minutes=30
rest_duration_minutes=5
snooze_duration_minutes=5
idle_threshold_seconds=10
autostart=1
user_name=昵称
character_image_file=
music_enabled=1
music_play_seconds=60
music_file=
```

- 读取：文件不存在 → 使用默认值并立即写入；单字段非法 → 仅该字段回退默认值，其余保留；缺少 `user_name` 时使用“昵称”。
- `user_name` 写入前去除控制字符与首尾空格，超过 20 个字符截断；空值回退“昵称”。
- 写入：先写 `config.ini.tmp`，再 `MoveFileEx(..., MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` 原子替换，避免断电损坏。
- `character_image_file` 启动时校验存在性与格式；不合法则清空该字段并回退内置形象。
- `music_file` 播放前校验扩展名、存在性、大小与 WAV 头；不合法则回退内置音乐，配置字段保留到用户下次选择（避免误删）。
- 存储预算：配置文件 ≤2 KB；未设置自定义形象时应用数据目录（配置 + 标记）≤16 KB；设置后用户形象单文件 ≤10 MB、用户音乐单文件 ≤5 MB；不写日志、不生成缩略图或额外缓存。
- `autostart` 与注册表保持一致：启动时若为 1 则确保注册表项存在并指向当前 exe 路径；为 0 则删除该项。

## 9. 打包、自启与单实例

- 发布形态：默认由 AI 智能体按 README 提示词自动安装；手动备用为单一安装包 `RelaxationReminder-Setup-<version>.exe`（Inno Setup 6 编译，每用户安装，无需管理员权限），内含单个核心程序 `RelaxationReminder.exe`（无 DLL、无外部资源目录）。不提供便携版，避免从压缩包临时目录运行、自启路径失效和升级残留。
- 安装目录：`%LOCALAPPDATA%\Programs\RelaxationReminder`；开始菜单快捷方式必装，桌面快捷方式可选（默认不勾选）。
- 版本资源：exe 写入 `FileVersion 0.11.25.0`、`ProductVersion 0.11.25.0` 和应用图标（与默认形象一致的琥珀色圆角方块）；托盘与任务栏使用同一图标。
- 升级：同 AppId 覆盖安装；安装前向已运行实例的隐藏窗口发送 `WM_CLOSE`，等待其退出后替换文件。用户配置与自定义形象不受影响。
- 卸载：删除程序文件、快捷方式、卸载项与自启注册表值；默认保留 `%APPDATA%\RelaxationReminder` 下的配置和自定义形象，重装后继续生效。
- 签名：不做代码签名（2026-11-06 决定）。首次运行安装包可能出现 SmartScreen“Windows 已保护你的电脑”，需用户点击“更多信息 → 仍要运行”；README 提供说明。未签名也可能触发少数杀软误报。
- 部署逻辑：**默认安装方式为 AI 智能体自动安装**——用户复制 README 中的提示词，智能体通过 GitHub API 获取最新 Release、下载 `RelaxationReminder-Setup-*.exe`、校验 SHA-256、静默安装、显式启动并验证；手动安装包作为备用方式保留。
- 智能体安装契约：发布资产名必须匹配 `RelaxationReminder-Setup-*.exe`，Release 说明必须包含安装包 SHA-256；安装器支持 Inno Setup 标准参数 `/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /LOG=<路径>`，退出码 0 或 3010 视为成功；`[Run]` 带 `skipifsilent`，静默安装不会自动启动，需在安装完成后单独启动 exe。提示词要求只从官方 Release 下载、用 `Get-FileHash` 或 `certutil` 校验 SHA-256、以当前用户安装，并验证进程与配置文件；不得关闭 Defender/SmartScreen、添加排除项或请求管理员权限。卸载使用 `unins000.exe /VERYSILENT /SUPPRESSMSGBOXES`，仍保留 `%APPDATA%\RelaxationReminder`。
- 编译：Release x64，`/MT /O2`，保留默认 `/GS` 栈保护，关闭 RTTI；异常仅用于启动期致命错误。
- 清单：`asInvoker`、Per-Monitor V2 DPI、Common Controls v6、支持 Win11。
- 自启：`HKCU\Software\Microsoft\Windows\CurrentVersion\Run`，值名 `RelaxationReminder`，数据为带引号的 exe 绝对路径。安装器不创建该值，由程序按配置管理；程序每次启动且在自启开启时重写路径，确保移动或重装后仍指向当前 exe。
- 首次运行默认写入自启项；用户在设置/托盘关闭后删除，之后启动不再自动恢复。
- 单实例：命名互斥体 `Local\RelaxationReminder`；重复启动时向已有实例发消息打开设置，然后退出。
- 关闭提醒窗口不退出进程；只有托盘“退出”或安装/卸载程序请求结束时，常驻进程才退出。

## 10. macOS 方案（本期不实现）

- 形态：菜单栏常驻应用（`NSStatusItem`），`LSUIElement=true`，不显示 Dock 图标。
- 窗口：无边框 `NSPanel`，`level = .statusBar`，`collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary]`，`orderFrontRegardless()` 显示且不激活应用。
- 空闲检测：`CGEventSourceSecondsSinceLastEventType(.combinedSessionState, .init(rawValue: ~0))` 获取任意输入空闲秒数，阈值同为 10 秒。
- 扩大：复用 `core/growth` 的曲线，20 Hz `NSTimer` 调整窗口 frame；显示器工作区用 `NSScreen.visibleFrame`。
- 自启：macOS 13+ 使用 `SMAppService.mainApp.register()/unregister()`。
- 配置：`~/Library/Application Support/RelaxationReminder/config.ini`，格式与 Windows 一致。
- 体积：Swift 运行库自 macOS 10.14.5 起随系统提供；`-Osize` + 死代码剥离后目标 ≤1 MB，需在 Mac 上实测。若超标，改用 ObjC/ObjC++ 实现窗口层。
- 自定义形象：`NSImage` / `NSBitmapImageRep` 读取同一文件，GIF 按帧循环；文件存 `~/Library/Application Support/RelaxationReminder/character/`，格式与上限同 Windows。
- 构建需要 macOS + Xcode，当前 Windows 环境无法编译和验收。

## 11. iOS 方案（本期不实现）

- 系统限制：不能后台任意弹窗、不能全局检测键鼠/触控空闲、后台计时由系统调度，因此**桌面端的“空闲判定 + 指数扩大”模型不适用于 iOS**。
- 提醒方式：
  - 前台：应用内浮层，固定展示时长，提供“延后”和“关闭”。
  - 后台/锁屏：`UNUserNotificationCenter` 本地通知，注册 `SNOOZE` 操作；点击延后重排一条通知，每次提醒最多一次。
- 权限被拒降级：仅在应用前台显示浮层，并在设置页展示文案“未开启通知权限，只能在应用内提醒。请到 设置 > 通知 > 休息提醒 开启。”
- 后台计时：按本地通知触发时间调度；回到前台时根据“上次最终消失时间”重算下一次提醒，超过则立即提醒一次，不补发。
- 体积：iOS 安装包包含必选图标与 bundle 元数据，1 MB 上限无法满足；已确认允许例外，按 iOS 单列记录实际大小。
- 自定义形象：`PhotosPicker` 选择图片并复制到应用沙盒，GIF 用 `UIImage.animatedImage` 播放；受相册权限与存储限制。
- 构建需要 macOS + Xcode，本期不实现。

## 12. 验收与测量方案

### 12.1 发布产物大小

- 统计范围：核心 exe 为 `RelaxationReminder.exe` 本体 + 内嵌资源；安装包为 `RelaxationReminder-Setup-<version>.exe` 单文件。均不含 Windows 自带的系统 DLL/运行库，不含用户自定义形象文件。不依赖 VC++ Redistributable。
- 测量：`(Get-Item .\RelaxationReminder.exe).Length` 与 `(Get-Item .\dist\RelaxationReminder-Setup-<version>.exe).Length`，连续构建 3 次取最大值。
- 目标：核心 exe ≤450 KB（硬指标 ≤1 MB）；安装包 ≤3 MB（单独记录，不计入 1 MB 判定）。当前构建含内置音乐实测 exe 约 360 KB（369,152 B）；安装包发布时实测记录。

### 12.2 内存占用

- 基准环境：本机 Windows 11 Pro 26200、63.6 GB 内存（1%≈651 MB）；另在 8 GB 虚拟机验证（1%≈82 MB）。
- 自定目标：常驻空闲私有工作集 ≤20 MB；提醒窗口显示/扩大期间 ≤30 MB；加载自定义形象时 ≤50 MB（仍低于 8 GB 机器 1% 的 82 MB）。
- 工具：Process Explorer 17.x（Private Bytes / Working Set），或 PowerShell：
  - `(Get-Process RelaxationReminder).PrivateMemorySize64 / 1MB`
  - `Get-Counter '\Process(*)\Working Set - Private'`（性能计数器实例名会截断为 15 字符）。
- 采样：启动后预热 5 分钟，空闲状态每 30 秒采样一次、持续 30 分钟，记录平均值与最大值；提醒显示期间每 5 秒采样一次、持续 5 分钟。
- 判定：默认形象场景最大值 ≤ min(物理内存 1%, 20 MB)；加载自定义形象场景 ≤ min(物理内存 1%, 50 MB)。
- 音乐：待机不加载 winmm（延迟加载），零增量；播放内置音乐的瞬时增量约 0.2~0.4 MB（winmm 映射 + 94 KB PCM + 4 个 40 ms 输出缓冲）；用户 5 MB WAV 瞬时增量约 ≤6 MB。测量时在播放开始后以 50 ms 间隔轮询 `PrivateMemorySize64` 取峰值；自定义形象与音乐同时存在时目标 ≤60 MB。

### 12.3 空闲 CPU

- 方法：读取 `GetProcessTimes` 的累计 CPU 时间，空闲 10 分钟后计算增量：
  `CPU% = ΔCPU秒 / 600 × 100%`（单核口径）。
- 目标：空闲 ≤0.1%（单核）；窗口扩大期间 ≤1%（单核，20 Hz 重绘）。
- 可用 `typeperf "\Process(*)\% Processor Time"` 交叉验证。

### 12.4 存储占用

- 默认：配置 ≤2 KB，`%APPDATA%\RelaxationReminder` 总大小 ≤16 KB；设置自定义形象后：用户形象单文件 ≤10 MB、目录总大小 ≤10.1 MB；设置音乐后：用户音乐单文件 ≤5 MB；两者同时设置时目录总大小 ≤15.1 MB。无日志、无缩略图缓存。
- 测量：使用 7 天后统计目录大小。

### 12.5 功能验收清单

| 编号 | 场景 | 预期 |
|---|---|---|
| T1 | 启动后等待 | 满 `reminder_interval_minutes` 才弹出，不立即弹 |
| T2 | 弹出位置 | 出现在鼠标所在显示器，光标附近，不抢键盘焦点 |
| T3 | 键鼠保持活动 | 窗口持续扩大，约 60 秒达到工作区 30% 后保持，不自动消失 |
| T4 | 键鼠空闲达到配置阈值 | 开始休息倒计时；窗口继续按曲线扩大 |
| T5 | 休息中恢复操作 | 计时暂停，窗口继续扩大；再空闲达到阈值后继续计时 |
| T6 | 休息倒计时归零 | 窗口消失，下一次间隔从此刻起算 |
| T7 | 首次弹窗点延后 | 二次确认后窗口消失，`snooze_duration_minutes` 后重弹，尺寸重置 |
| T8 | 重弹后延后按钮 | 按钮存在但置灰不可点 |
| T9 | 重弹后休息/关闭 | 规则同首次；关闭确认后进入下一轮等待 |
| T10 | 手动关闭 | 二次确认后消失并重新计时；取消则保持显示 |
| T11 | 休眠唤醒跨过间隔 | 唤醒后只提醒一次，不补发 |
| T12 | 多显示器/不同 DPI | 窗口位于光标显示器，尺寸与清晰度正常 |
| T13 | 设置非法值 | 拒绝保存并提示；合法值持久化，下一周期生效 |
| T13a | 设置面板入口/重置 | 提醒窗口左下角“设置”与托盘菜单均可打开；拖动条按档位即时生效且右侧数值无重影；点“恢复默认值”即恢复 30/5/5、昵称 并立即保存生效，面板保持打开，空闲阈值与开机自启不变 |
| T13b | 显示名称 | 标题显示“{user_name}，休息一下”；修改后立即生效，空值回退“昵称” |
| T14 | 自启开关 | 勾选写注册表，取消删除；重启后状态保持 |
| T15 | 重复启动 | 不产生第二个进程，打开已有实例设置 |
| T16 | 自定义静态图 | 选择后按比例显示，重启后保持；恢复默认后回到内置方块 |
| T17 | 自定义动图/异常文件 | GIF 循环播放；损坏、超限或格式不支持时回退内置方块并提示 |
| T18 | 休息结束进入音乐 | 倒计时归零后窗口保持打开、开始播放音乐、标题变为“生产队的驴都不敢这么歇！”；播满 `music_play_seconds` 后关闭并重新计时 |
| T19 | 音乐淡出与停止 | 播放期间动键鼠音乐 1 s 内淡出、窗口 1 s 后关闭；手动关闭、暂停计时、休眠、退出时立即停止；音乐期间“延后”置灰 |
| T20 | 选择/恢复用户音乐 | 5 MB 内 PCM WAV 复制到 `sound` 目录并重启保持；恢复默认后删除文件并回退内置音乐 |
| T21 | 非法/超大/压缩音频 | 拒绝并提示；用户文件被删除或损坏时回退内置音乐 |
| T22 | 音乐内存 | 待机不加载 winmm；播放峰值符合预算，关闭窗口后回落 |

## 13. 风险、假设与待确认

1. **提醒可能长时间不消失**：用户一直有键鼠输入时，窗口停在 30% 面积等待，不会自动关闭。这是新需求的直接结果，已确认符合预期。
2. **空闲阈值范围**：`idle_threshold_seconds` 允许 1–3600 秒；过小会误判短暂停顿，建议 5–120 秒但不强制。该参数不在设置面板显示，仅可通过配置文件修改。
3. **“休息”的语义**：仅以系统键鼠空闲判定；用户阅读长文、看视频时不动键鼠也会被判定为休息，符合当前定义。
4. **触摸/手写笔**：Windows 触摸通常产生鼠标输入，`GetLastInputInfo` 应能感知；需在触摸设备上补测。
5. **全屏独占程序/UAC 安全桌面**：置顶窗口可能不可见，安全桌面期间无法显示；恢复后按 F9 规则处理。
6. **代码签名**：不做代码签名（2026-11-06 决定，证书时间与成本不可接受）；首次运行安装包可能触发 SmartScreen，需用户手动放行，杀软误报风险自担。
7. **iOS 1 MB 产物**：已确认允许例外，按 iOS 单列记录实际大小。
8. **macOS/iOS 无法在当前环境验证**：本期只交付设计，构建与验收需 Mac 环境。
9. **自定义动图的 CPU/内存开销**：逐帧解码和大尺寸图片可能抬高占用，已通过 10 MB / 1024×1024 / 500 帧上限约束；验收时需单独测量该场景。
10. **不可信图片文件**：只做 GDI+ 解码，不执行内容；加载前校验尺寸、帧数与文件大小，防止畸形文件导致崩溃或内存膨胀。

## 14. 里程碑

| 阶段 | 内容 | 产出 |
|---|---|---|
| M0 | 需求确认、技术方案 | 本文档、AGENTS.md 更新 |
| M1 | Win32 骨架：消息循环、托盘、配置、状态机、自启、单实例 | 已完成：可运行 exe（约 146 KB），状态机单测通过 |
| M2 | 提醒窗口、指数扩大、空闲判定、暂停恢复、延后 | 下一步：核心行为可验收 |
| M3 | 设置界面、DPI/多显示器适配、托盘交互完善 | 功能完整 |
| M4 | 体积/内存/CPU/存储测量与功能清单 | 验收记录 |
| M5 | macOS 实现（需 Mac） | .app 与测量记录 |
| M6 | iOS 实现（需 Mac） | .ipa 与降级验证 |

## 15. 已确认决策（2026-11-06）

1. 用户持续使用键鼠时，窗口停在 30% 面积等待，不自动消失；这是预期行为。
2. `idle_threshold_seconds` 作为第 4 个参数，默认 10 秒，可配置。
3. iOS 的 1 MB 产物指标允许例外，按平台单列记录。
4. 托盘“立即休息”为正式功能，点击立即显示提醒窗口；测试模式不再单独提供“立即提醒”项，`--remind-now` 启动参数保留。
5. 大小限制放宽（2026-11-06）：核心 exe 维持 ≤1 MB，安装包单独统计，不计入 1 MB 判定，设计目标 ≤3 MB。
6. 不做代码签名（2026-11-06 修订）：证书成本与时间不可接受；SmartScreen 提示由用户手动放行，README 提供说明。
7. 首个公开发行版本号定为 **0.11.25.0**（2026-11-06），发布时使用。
8. 分发方式（2026-11-06，2026-11-09 修订）：默认部署方式为 AI 智能体自动安装（README 提供可复制提示词），手动备用为每用户安装包（Inno Setup），安装到 `%LOCALAPPDATA%\Programs\RelaxationReminder`，不提供便携版。
9. 卸载默认保留 `%APPDATA%\RelaxationReminder` 中的配置和自定义形象，重装后继续生效。
10. 安装器不创建自启项，由程序按默认开启策略写入；程序启动时重写路径以自愈。
11. 设置面板改为所见即所得（2026-11-06）：移除“保存/取消”，字段修改立即生效；显示名称为空或含控制字符时使用“牛马”。
12. 开机自启只保留在托盘菜单（2026-11-06）：设置面板移除该行，“恢复默认值”不改动自启状态。
13. 设置面板档位化（2026-11-06）：三个时间参数改为拖动条（提醒间隔 20–60 步长 10，休息/延后 5/10/15），移除右侧说明与红色校验提示；空闲阈值仅保留配置文件入口，重置不改动该值；全界面统一使用 Microsoft YaHei UI。
14. 休息结束音乐（2026-11-09，同日修订）：取消 `music_lead_seconds`；休息倒计时归零后开始播放，时长由 `music_play_seconds`（30–120，默认 60）控制，面板拖动条可改；播放期间窗口保持打开、标题改为“生产队的驴都不敢这么歇！”；动键鼠即淡出并关闭；音乐关闭时倒计时结束即关闭窗口。内置音乐为程序合成的 2 秒片段（24 kHz 单声道 PCM，内嵌 exe，复刻参考提示音前 2 秒的 1049 Hz 铃声），循环播放且每遍后静音 3 秒（2 秒音乐、3 秒停顿）；用户可选 PCM WAV（≤5 MB）。播放由 `PlaySound` 改为 `waveOut` 流式播放以支持淡出，仍延迟加载 winmm；不提供单条音量。

## 16. 待确认事项

1. 界面语言是否仅中文？
2. 自定义形象是否需要支持 WebP/APNG，或维持当前 PNG/JPEG/BMP/GIF 白名单？


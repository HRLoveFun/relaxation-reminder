# 休息提醒 · Relaxation Reminder

> 一个会在鼠标旁慢慢变大的 Windows 11 休息提醒。到点提醒，真正离开键鼠才开始休息倒计时；轻量常驻、不抢焦点，不打断工作节奏。

> **默认安装方式：让 AI 智能体安装。** 复制[一段提示词](#默认方式ai-智能体安装)发给 Claude Code、Codex CLI 等能执行本地命令的智能体，自动完成下载、校验、静默安装和启动，无需手动操作。

[![Release](https://img.shields.io/github/v/release/HRLoveFun/relaxation-reminder?display_name=tag&sort=semver)](https://github.com/HRLoveFun/relaxation-reminder/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2011%20x64-0078D4)](#系统要求)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

**关键词：** Windows 11 休息提醒 · 久坐提醒 · 护眼提醒 · 定时休息 · 番茄钟替代 · 轻量常驻 · 托盘工具 · 开机自启 · 自定义形象 · GIF 动图

[下载最新版](https://github.com/HRLoveFun/relaxation-reminder/releases/latest) · [功能](#功能) · [安装](#安装) · [智能体安装](#默认方式ai-智能体安装) · [手动安装](#手动安装备用) · [使用说明](#使用说明) · [配置](#配置) · [常见问题](#常见问题)

![设置面板](docs/settings-panel.png)

## 为什么需要它

久坐、长时间盯屏幕时，普通的通知弹窗很容易被忽略：看到了，点掉，继续工作。这个工具换了一种提醒方式：

- **提醒会“长大”**：窗口从鼠标位置出现，60 秒内指数扩大到工作区面积的 30%，持续占据视野，直到你真正休息或手动关闭。
- **只有离开键鼠才算休息**：键鼠空闲满设定阈值（默认 10 秒）才开始休息倒计时；中途回来操作，倒计时暂停，再次离开后继续。
- **不抢键盘焦点**：提醒窗口置顶但不会抢走输入焦点，正在打字时不会打断输入。
- **轻量常驻**：单文件 exe 约 240 KB，空闲内存约 17 MB，无第三方运行时、无广告、无联网请求。

适合需要长时间坐在电脑前工作、学习、写代码，希望被“温和但无法忽略”地提醒起来活动一下的人。

## 功能

- **定时休息提醒**：按设定间隔弹出提醒，间隔从本次提醒最终消失后开始计算。
- **鼠标位置出现 + 指数扩大**：初始 320×320（按 DPI 缩放），面积按指数曲线增长，60 秒达到鼠标所在显示器工作区的 30% 后保持。
- **智能休息判定**：键鼠无输入满 `idle_threshold_seconds`（默认 10 秒）判定开始休息，启动 5 分钟休息倒计时；期间有输入则暂停，再次空闲后继续。
- **延后一次**：每次提醒可延后一次，默认 5 分钟；重新弹出后回到初始尺寸，延后按钮置灰。
- **手动关闭**：关闭前二次确认，并提示下次提醒时间。
- **托盘常驻**：支持“立即休息”、设置、选择形象、恢复默认形象、开机自启开关和退出。
- **开机自启**：默认开启，可在托盘菜单关闭；每次启动按当前 exe 路径自动修复自启项。
- **自定义形象**：支持 PNG、JPEG、BMP、GIF（含动画），未设置或加载失败时回退默认卡通方块。
- **即时生效设置**：提醒间隔、休息时长、延后时长使用拖动条，显示名称所见即所得，没有“保存/取消”。
- **异常恢复**：休眠唤醒、锁屏恢复后若已超过提醒时间则立即提醒一次；睡眠期间不误计休息时间。
- **单实例**：重复启动会唤起已运行实例的设置面板，不会产生多个托盘图标。

## 它是怎么工作的

1. 启动后常驻托盘，显示数秒“休息提醒已启动”提示。
2. 等待 `reminder_interval_minutes`（默认 30 分钟）后，提醒窗口从鼠标位置弹出并开始扩大。
3. 如果你一直在使用键鼠，窗口继续扩大并停留在 30% 工作区大小，不会自动消失。
4. 键鼠无输入满 10 秒，判定你开始休息，启动 5 分钟倒计时；窗口仍按原曲线扩大。
5. 倒计时期间回来操作，倒计时暂停；再次离开 10 秒后从暂停处继续。
6. 倒计时结束或手动关闭后，窗口消失，重新等待 30 分钟后再提醒。

> 延后会使下一次提醒整体顺延：下一次计时从本次提醒“最终消失”后开始。

## 安装

默认安装方式是由 **AI 智能体**完成：把一段提示词交给 Claude Code、Codex CLI、Cursor、Windsurf 等能执行本地命令的智能体，它会自动下载、校验、静默安装并启动，无需手动下载或点击安装向导。手动安装作为备用方式保留。

### 默认方式：AI 智能体安装

前提：Windows 11 x64、智能体有本地命令执行权限、能访问 GitHub。安装包未做代码签名，提示词要求智能体只从官方 Release 下载、校验 SHA-256，且不得关闭安全软件或绕过系统提示。

复制下面整段提示词发给智能体，你只需要在最后确认托盘图标：

```text
请在我的 Windows 11 x64 电脑上安装「休息提醒（Relaxation Reminder）」，并验证安装结果。这是一个常驻托盘的休息提醒工具，安装后默认每 30 分钟提醒一次。全程使用命令行完成，不要让我手动点击安装向导。

安全约束（必须遵守）：
1. 只从官方仓库 https://github.com/HRLoveFun/relaxation-reminder 的 Releases 下载，不使用任何第三方镜像或转载地址。
2. 安装包未做代码签名。不要关闭 Windows Defender、SmartScreen，不要添加杀毒排除项，也不要用任何方式绕过系统安全提示。
3. 以当前用户权限安装，不要请求管理员权限。如果某一步必须提权，停下来说明原因并等我确认。
4. 下载失败、哈希不一致或安装退出码非 0 时停止，把错误原文告诉我，不要自行换源重试。
5. 不要修改我的配置文件或系统设置（程序自身默认写入的开机自启项除外）。

执行步骤：
1. 确认系统为 Windows 11 x64：
   [System.Environment]::Is64BitOperatingSystem
   (Get-CimInstance Win32_OperatingSystem).Caption
   不满足就停止并告诉我。
2. 读取最新 Release 并找到安装包：
   $release = Invoke-RestMethod -Uri 'https://api.github.com/repos/HRLoveFun/relaxation-reminder/releases/latest' -Headers @{ 'User-Agent' = 'RelaxationReminder-Installer' }
   $asset = $release.assets | Where-Object { $_.name -like 'RelaxationReminder-Setup-*.exe' } | Select-Object -First 1
   如果找不到安装包，停止并报告。
3. 下载到临时目录，并移除“来自 Internet”标记：
   $dir = Join-Path $env:TEMP 'RelaxationReminder-Install'
   New-Item -ItemType Directory -Force -Path $dir | Out-Null
   $setup = Join-Path $dir $asset.name
   curl.exe -L --fail --retry 3 -o $setup $asset.browser_download_url
   Unblock-File -Path $setup
4. 计算 SHA-256，并在 $release.body 中查找 SHA-256 对照：
   (Get-FileHash $setup -Algorithm SHA256).Hash.ToLower()
   如果 Release 说明没有给出哈希，记录计算结果并在最终报告中列出；不一致则停止。
5. 静默安装（Inno Setup 参数；安装器会先请求已运行的旧版本退出，配置与自定义形象保留）：
   $log = Join-Path $dir 'install.log'
   $p = Start-Process -FilePath $setup -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',('/LOG="{0}"' -f $log)) -PassThru -Wait
   退出码 0 或 3010 视为成功；其他值请读取 $log 后报告。
6. 启动程序（静默安装不会自动启动）：
   $exe = Join-Path $env:LOCALAPPDATA 'Programs\RelaxationReminder\RelaxationReminder.exe'
   Start-Process $exe
7. 验证：
   - Get-Process RelaxationReminder 存在；
   - Test-Path (Join-Path $env:APPDATA 'RelaxationReminder\config.ini') 为 True；
   - (Get-Item $exe).VersionInfo.FileVersion 与 Release 版本一致；
   - 托盘图标无法用脚本可靠确认，请让我看一下右下角托盘。
8. 最终报告：安装版本、安装路径、配置文件路径、下载文件的 SHA-256、安装日志路径，以及如何打开设置（左键双击托盘图标）和卸载方式。

如果安装失败且提示文件被占用，请提示我通过托盘菜单「退出」后重试，不要强制结束进程。
```

提示：如果环境的 `Get-FileHash` 不可用，可用 `certutil -hashfile <文件> SHA256` 代替；两条命令输出一致即可。

### 手动安装（备用）

以下是智能体执行的同一套操作，供不想使用智能体时手动完成：

1. 打开 [Releases](https://github.com/HRLoveFun/relaxation-reminder/releases/latest)，下载 `RelaxationReminder-Setup-0.11.25.0.exe`。
2. 双击安装。安装为当前用户，无需管理员权限，默认目录 `%LOCALAPPDATA%\Programs\RelaxationReminder`。
3. 如果 Windows 显示 SmartScreen 提示，点击“更多信息”→“仍要运行”（安装包未做代码签名）。
4. 安装完成后程序会自动启动，并常驻系统托盘。
5. 想立刻看效果：右键托盘图标 → **立即休息**。
6. 想修改间隔：提醒窗口左下角“设置”，或右键托盘图标 → **设置**。

安装完成后默认每 30 分钟提醒一次，休息 5 分钟，延后 5 分钟，键鼠空闲 10 秒判定为开始休息。

#### 命令行静默安装

已经下载好安装包、只想跳过向导时，可以在安装包所在目录执行：

```powershell
Start-Process -FilePath '.\RelaxationReminder-Setup-0.11.25.0.exe' `
  -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-' -Wait
Start-Process "$env:LOCALAPPDATA\Programs\RelaxationReminder\RelaxationReminder.exe"
```

静默安装会跳过安装完成后的“启动程序”，因此需要第二行手动启动。卸载：

```powershell
Start-Process "$env:LOCALAPPDATA\Programs\RelaxationReminder\unins000.exe" `
  -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART' -Wait
```

卸载仍保留 `%APPDATA%\RelaxationReminder` 中的配置与自定义形象。

## 使用说明

### 提醒窗口

| 操作 | 说明 |
|---|---|
| **延后** | 二次确认后关闭当前窗口，`snooze_duration_minutes`（默认 5 分钟）后重新弹出；每次提醒只能延后一次，重新弹出后按钮置灰。 |
| **关闭** | 二次确认后立即结束本次提醒，并从现在开始重新等待 `reminder_interval_minutes`。 |
| **设置** | 打开参数面板；面板独立置顶，不随提醒窗口消失。 |
| 等待休息 | 键鼠空闲满阈值后自动进入休息倒计时，倒计时结束窗口自动消失。 |

### 托盘菜单

| 菜单项 | 作用 |
|---|---|
| **设置** | 打开参数面板（左键双击托盘图标也可）。 |
| **立即休息** | 立即弹出提醒窗口；如果提醒已在显示，则不会重置尺寸或计时。 |
| **选择形象图片** | 选择本地 PNG/JPEG/BMP/GIF 作为提醒形象。 |
| **恢复默认形象** | 删除自定义形象，回到默认卡通方块。 |
| **开机自启** | 勾选/取消开机自动启动，立即生效。 |
| **退出** | 退出程序；本次运行结束。 |

### 设置面板

所有修改即时生效并写入配置，没有“保存/取消”按钮。

| 项目 | 可选值 | 说明 |
|---|---|---|
| 提醒间隔 | 20 / 30 / 40 / 50 / 60 分钟 | 本次提醒最终消失到下一次提醒的等待时间。 |
| 休息时长 | 5 / 10 / 15 分钟 | 进入休息后需要保持空闲的时长。 |
| 延后时长 | 5 / 10 / 15 分钟 | 点击延后后重新弹出的等待时间。 |
| 显示名称 | 1–20 个字符 | 标题显示“{名称}，休息一下”；默认“芬宝”，为空或含控制字符时使用“牛马”。 |
| 恢复默认值 | — | 立即恢复 30/5/5 和“芬宝”；不改变空闲阈值与开机自启。 |

`idle_threshold_seconds`（空闲阈值，默认 10 秒）不在面板显示，只能通过配置文件修改，详见[配置](#配置)。

### 自定义形象

1. 右键托盘图标 → **选择形象图片**。
2. 选择 PNG、JPEG、BMP 或 GIF（支持动画 GIF）。
3. 图片会复制到 `%APPDATA%\RelaxationReminder\character`，之后启动自动加载。
4. 右键托盘 → **恢复默认形象** 可随时还原。

限制：单文件 ≤10 MB，解码后单帧 ≤1024×1024，动图 ≤500 帧。文件缺失、格式不支持或解码失败时自动回退默认方块。

### 开机自启与卸载

- 开机自启默认开启，可在托盘菜单关闭；程序每次启动会按当前 exe 路径重写 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\RelaxationReminder`。
- 从“设置 → 应用 → 已安装的应用”卸载即可。
- 卸载默认**保留** `%APPDATA%\RelaxationReminder`（配置与自定义形象），重装后继续生效；如需彻底清除，手动删除该目录。
- 覆盖安装会先请求正在运行的实例退出，再替换程序文件。

## 系统要求

- Windows 11 x64
- 无需管理员权限、无需安装 .NET / Visual C++ 运行库（静态链接 CRT）
- 安装包最低系统版本为 Windows 10 1809；开发与验收以 Windows 11 为准

## 配置

配置文件：`%APPDATA%\RelaxationReminder\config.ini`（UTF-16 LE，启动时缺失则自动创建）。

| 键 | 默认值 | 说明 |
|---|---|---|
| `reminder_interval_minutes` | 30 | 提醒间隔，1–1440；设置面板限制为 20–60 |
| `rest_duration_minutes` | 5 | 休息时长，1–1440；设置面板限制为 5/10/15 |
| `snooze_duration_minutes` | 5 | 延后时长，1–1440；设置面板限制为 5/10/15 |
| `idle_threshold_seconds` | 10 | 空闲判定阈值，1–3600；仅配置文件可改 |
| `autostart` | 1 | 是否开机自启，0/1 |
| `user_name` | 芬宝 | 显示名称，1–20 个字符；为空或含控制字符时使用“牛马” |
| `character_image_file` | 空 | 自定义形象文件名；仅文件名，不含路径 |

非法数值回退默认值。配置文件中的时间值若不在设置面板档位内，打开面板时会就近归入对应档位并保存。

## 从源码构建

环境：

- Windows 11 x64
- Visual Studio 2019（v142 工具集）与 Windows 10/11 SDK
- 安装“使用 C++ 的桌面开发”工作负载
- 生成安装包需要 [Inno Setup 6](https://jrsoftware.org/isdl.php)

```bat
build.bat
build.bat test
build.bat installer
```

- `build.bat`：构建 release exe。
- `build.bat test`：构建测试版并运行状态机单元测试。
- `build.bat installer`：构建 release 与安装包。

产物：

- `build\RelaxationReminder.exe`：单文件、静态 CRT、无第三方运行时依赖。
- `dist\RelaxationReminder-Setup-0.11.25.0.exe`：每用户安装包。

也提供 CMake 工程：

```bat
cmake -S . -B build-cmake -G "Visual Studio 16 2019" -A x64
cmake --build build-cmake --config Release
```

## 常见问题

**手动安装时 Windows 提示“Windows 已保护你的电脑”，无法安装？**
安装包未做代码签名。点击“更多信息”→“仍要运行”即可。智能体安装提示词会先移除“来自 Internet”标记并静默安装，通常不会弹出该提示。

**为什么提醒窗口越来越大？**
这是设计的一部分：如果提醒被忽略，窗口会持续扩大以占据视野，最多到工作区面积的 30%。开始休息、延后或关闭后即停止。

**为什么休息倒计时不开始？**
需要键鼠完全无输入满 `idle_threshold_seconds`（默认 10 秒）。移动鼠标、按键都会暂停倒计时；再次空闲满 10 秒后继续。

**“延后”按钮为什么是灰的？**
每次提醒只能延后一次。重新弹出的窗口仍可正常休息或关闭。

**提醒太频繁/太少？**
打开设置面板调整“提醒间隔”（20–60 分钟），或直接修改配置文件的 `reminder_interval_minutes`。

**卸载后配置还在？**
这是有意保留的，方便重装后继续使用。彻底删除请移除 `%APPDATA%\RelaxationReminder`。

**可以让 AI 智能体帮我安装吗？**
这就是默认安装方式。复制[智能体安装](#默认方式ai-智能体安装)中的提示词，发给 Claude Code、Codex CLI、Cursor 等能执行本地命令的智能体即可；它会从官方 Release 下载、校验 SHA-256、静默安装并启动。提示词要求智能体不关闭安全软件、不请求管理员权限，遇到哈希不一致或安装失败时必须停止并报告。不想使用智能体时，可按[手动安装](#手动安装备用)操作。

**支持 macOS / iOS 吗？**
当前只实现 Windows 11 桌面端，macOS 与 iOS 在规划中。

## 项目文档

- [AGENTS.md](AGENTS.md)：需求与功能定义
- [TECH_DESIGN.md](TECH_DESIGN.md)：技术方案、状态机、测量方法
- [PRODUCT_ASSESSMENT.md](PRODUCT_ASSESSMENT.md)：竞品与立项评估
- [Releases](https://github.com/HRLoveFun/relaxation-reminder/releases)：下载与更新日志

## 许可证

[MIT](LICENSE)

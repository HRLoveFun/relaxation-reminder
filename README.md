# 休息提醒 · Relaxation Reminder

> 一个会在鼠标旁慢慢变大的休息提醒。到点提醒，真正离开键鼠才开始休息倒计时；轻量常驻、不抢焦点。

> **默认安装方式：AI 智能体安装。** 把[提示词](#默认方式ai-智能体安装)交给 Claude Code、Codex CLI 等能执行本地命令的智能体，自动完成下载、校验、静默安装和启动。

[![Release](https://img.shields.io/github/v/release/HRLoveFun/relaxation-reminder?display_name=tag&sort=semver)](https://github.com/HRLoveFun/relaxation-reminder/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078D4)](#系统要求)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

**关键词：** 休息提醒 · 久坐提醒 · 护眼提醒 · 定时休息 · 轻量常驻 · 托盘工具 · 开机自启 · 自定义形象

[下载最新版](https://github.com/HRLoveFun/relaxation-reminder/releases/latest) · [功能](#功能) · [安装](#安装) · [使用说明](#使用说明) · [配置](#配置) · [常见问题](#常见问题)

## 为什么需要它

普通通知很容易被忽略：看到了，点掉，继续工作。这个工具换成一种无法忽略的方式：

- **提醒会“长大”**：窗口从鼠标位置出现，60 秒内指数扩大到工作区面积的 30%，持续占据视野。
- **只有离开键鼠才算休息**：空闲满阈值（默认 10 秒）才开始休息倒计时；中途操作则暂停，再次离开后继续。
- **不抢焦点、轻量常驻**：置顶但不抢输入焦点；exe 约 240 KB，空闲内存约 17 MB，无第三方运行时与联网请求。

## 功能

- **定时提醒**：按间隔弹出，间隔从本次提醒最终消失后开始计算。
- **鼠标位置出现 + 指数扩大**：初始 320×320（随 DPI 缩放），60 秒达到鼠标所在显示器工作区的 30%。
- **休息判定**：键鼠空闲满阈值开始倒计时；有输入暂停，再次空闲继续。
- **延后一次**：每次提醒可延后一次，重弹后回到初始尺寸且按钮置灰。
- **手动关闭**：二次确认，并提示下次提醒时间。
- **暂停计时**：托盘可随时暂停/恢复全部计时，窗口保持不变。
- **托盘常驻**：设置、立即休息、暂停计时、形象、开机自启、退出。
- **开机自启**：默认开启，可关闭；每次启动按当前 exe 路径修复自启项。
- **自定义形象**：PNG/JPEG/BMP/GIF（含动画），加载失败回退默认方块。
- **即时设置**：时间参数用拖动条，没有“保存/取消”。
- **异常恢复**：唤醒后若已超时立即提醒一次；睡眠不计入休息时间。
- **单实例**：重复启动会唤起已运行实例的设置面板。

## 它是怎么工作的

1. 启动后常驻托盘，显示数秒“已启动”提示。
2. 等待 `reminder_interval_minutes`（默认 30 分钟）后弹出提醒并开始扩大。
3. 键鼠活跃时窗口继续扩大并停在 30% 工作区大小，不自动消失。
4. 空闲满 10 秒判定开始休息，启动 5 分钟倒计时；操作则暂停，再空闲继续。
5. 倒计时结束或手动关闭后窗口消失，重新等待 30 分钟。

> 延后会让下一次提醒整体顺延。

## 安装

默认由 AI 智能体安装，手动安装作为备用方式保留。

### 默认方式：AI 智能体安装

前提：64 位 Windows、智能体能执行本地命令并访问 GitHub。安装包未做代码签名，提示词要求智能体只从官方 Release 下载、校验 SHA-256，且不得关闭安全软件或绕过系统提示。

复制下面整段提示词发给智能体，最后确认托盘图标即可：

```text
请在我的 Windows x64 电脑上安装「休息提醒（Relaxation Reminder）」，并验证安装结果。这是一个常驻托盘的休息提醒工具，安装后默认每 30 分钟提醒一次。全程使用命令行完成，不要让我手动点击安装向导。

安全约束（必须遵守）：
1. 只从官方仓库 https://github.com/HRLoveFun/relaxation-reminder 的 Releases 下载，不使用任何第三方镜像或转载地址。
2. 安装包未做代码签名。不要关闭 Windows Defender、SmartScreen，不要添加杀毒排除项，也不要用任何方式绕过系统安全提示。
3. 以当前用户权限安装，不要请求管理员权限。如果某一步必须提权，停下来说明原因并等我确认。
4. 下载失败、哈希不一致或安装退出码非 0 时停止，把错误原文告诉我，不要自行换源重试。
5. 不要修改我的配置文件或系统设置（程序自身默认写入的开机自启项除外）。

执行步骤：
1. 确认系统为 64 位 Windows：
   [System.Environment]::Is64BitOperatingSystem
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
8. 最终报告：安装版本、安装路径、配置路径、下载文件的 SHA-256、安装日志路径，以及如何打开设置（左键双击托盘图标）和卸载方式。

如果安装失败且提示文件被占用，请提示我通过托盘菜单「退出」后重试，不要强制结束进程。
```

`Get-FileHash` 不可用时可用 `certutil -hashfile <文件> SHA256` 代替。

### 手动安装（备用）

1. 从 [Releases](https://github.com/HRLoveFun/relaxation-reminder/releases/latest) 下载 `RelaxationReminder-Setup-0.11.25.0.exe`。
2. 双击安装。当前用户安装，无需管理员权限，默认目录 `%LOCALAPPDATA%\Programs\RelaxationReminder`；SmartScreen 提示时选“更多信息”→“仍要运行”。
3. 安装后自动启动并常驻托盘。想立刻看效果：右键托盘图标 → **立即休息**。

默认每 30 分钟提醒一次，休息 5 分钟，延后 5 分钟，空闲 10 秒判定开始休息。

#### 命令行静默安装

```powershell
Start-Process -FilePath '.\RelaxationReminder-Setup-0.11.25.0.exe' `
  -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-' -Wait
Start-Process "$env:LOCALAPPDATA\Programs\RelaxationReminder\RelaxationReminder.exe"
```

静默安装会跳过安装后的自动启动，因此需要第二行。卸载：

```powershell
Start-Process "$env:LOCALAPPDATA\Programs\RelaxationReminder\unins000.exe" `
  -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART' -Wait
```

卸载默认保留 `%APPDATA%\RelaxationReminder` 中的配置与自定义形象。

## 使用说明

### 提醒窗口

| 操作 | 说明 |
|---|---|
| **延后** | 二次确认后关闭窗口，`snooze_duration_minutes`（默认 5 分钟）后重新弹出；每次提醒只能延后一次。 |
| **关闭** | 二次确认后立即结束本次提醒，并重新等待 `reminder_interval_minutes`。 |
| **设置** | 打开参数面板；面板独立置顶，不随提醒窗口消失。 |

空闲满阈值后自动进入休息倒计时，倒计时结束窗口消失。二次确认窗口打开期间，休息倒计时与窗口扩大暂停。

### 托盘菜单

| 菜单项 | 作用 |
|---|---|
| **设置** | 打开参数面板（左键双击托盘图标也可）。 |
| **立即休息** | 立即弹出提醒；已在显示时不做处理。会同时恢复计时。 |
| **暂停计时** | 暂停/恢复全部计时（等待间隔、休息倒计时与窗口扩大）。 |
| **选择形象图片** | 选择本地 PNG/JPEG/BMP/GIF 作为提醒形象。 |
| **恢复默认形象** | 删除自定义形象，回到默认方块。 |
| **开机自启** | 勾选/取消开机自动启动，立即生效。 |
| **退出** | 退出程序。 |

### 设置面板

修改即时生效并写入配置，没有“保存/取消”按钮。

| 项目 | 可选值 | 说明 |
|---|---|---|
| 提醒间隔 | 20 / 30 / 40 / 50 / 60 分钟 | 本次提醒消失到下一次提醒的等待时间。 |
| 休息时长 | 5 / 10 / 15 分钟 | 进入休息后需要保持空闲的时长。 |
| 延后时长 | 5 / 10 / 15 分钟 | 点击延后后重新弹出的等待时间。 |
| 显示名称 | 1–20 个字符 | 标题“{名称}，休息一下”；默认“昵称”，为空或含控制字符时用“牛马”。 |
| 恢复默认值 | — | 恢复 30/5/5 和“昵称”；不改变空闲阈值与开机自启。 |

`idle_threshold_seconds`（空闲阈值，默认 10 秒）只能通过配置文件修改，见[配置](#配置)。

### 自定义形象

托盘右键 → **选择形象图片**，图片复制到 `%APPDATA%\RelaxationReminder\character` 并自动加载；**恢复默认形象** 可还原。单文件 ≤10 MB，单帧 ≤1024×1024，动图 ≤500 帧；缺失或解码失败时回退默认方块。

### 开机自启与卸载

- 自启默认开启，可在托盘关闭；每次启动按当前 exe 路径重写 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\RelaxationReminder`。
- 从“设置 → 应用 → 已安装的应用”卸载；默认保留 `%APPDATA%\RelaxationReminder`，彻底清除需手动删除。
- 覆盖安装会先请求正在运行的实例退出。

## 系统要求

Windows 10 1809 及以上，64 位。无需管理员权限，无需 .NET / Visual C++ 运行库（静态链接 CRT）。

## 配置

配置文件：`%APPDATA%\RelaxationReminder\config.ini`（UTF-16 LE，缺失时自动创建）。

| 键 | 默认值 | 说明 |
|---|---|---|
| `reminder_interval_minutes` | 30 | 提醒间隔，1–1440；面板限制 20–60 |
| `rest_duration_minutes` | 5 | 休息时长，1–1440；面板限制 5/10/15 |
| `snooze_duration_minutes` | 5 | 延后时长，1–1440；面板限制 5/10/15 |
| `idle_threshold_seconds` | 10 | 空闲阈值，1–3600；仅配置文件可改 |
| `autostart` | 1 | 是否开机自启，0/1 |
| `user_name` | 昵称 | 显示名称，1–20 个字符；为空或含控制字符时用“牛马” |
| `character_image_file` | 空 | 自定义形象文件名；仅文件名，不含路径 |

非法数值回退默认值；面板档位外的历史值会在打开面板时就近归档并保存。

## 从源码构建

环境：64 位 Windows、Visual Studio 2019（v142）与 Windows SDK，安装“使用 C++ 的桌面开发”；打包需 [Inno Setup 6](https://jrsoftware.org/isdl.php)。

```bat
build.bat
build.bat test
build.bat installer
```

产物：`build\RelaxationReminder.exe`（单文件、静态 CRT）与 `dist\RelaxationReminder-Setup-0.11.25.0.exe`。也可用 CMake：

```bat
cmake -S . -B build-cmake -G "Visual Studio 16 2019" -A x64
cmake --build build-cmake --config Release
```

## 常见问题

**安装时提示“Windows 已保护你的电脑”？**
安装包未签名，选“更多信息”→“仍要运行”。智能体安装会先移除“来自 Internet”标记并静默安装。

**为什么提醒窗口越来越大？**
提醒被忽略时持续扩大以占据视野，最多到工作区的 30%；休息、延后、关闭或暂停计时后停止。

**为什么休息倒计时不开始？**
需要键鼠完全无输入满 `idle_threshold_seconds`（默认 10 秒）；操作会暂停倒计时。

**“延后”按钮为什么是灰的？**
每次提醒只能延后一次；重弹的窗口仍可休息或关闭。

**卸载后配置还在？**
有意保留以便重装后继续使用；彻底删除请移除 `%APPDATA%\RelaxationReminder`。

**可以让 AI 智能体帮我安装吗？**
这就是默认方式：把[提示词](#默认方式ai-智能体安装)交给能执行本地命令的智能体即可。

**支持 macOS / iOS 吗？**
当前只提供 Windows 桌面端，macOS 与 iOS 在规划中。

## 项目文档

- [AGENTS.md](AGENTS.md)：需求与功能定义
- [TECH_DESIGN.md](TECH_DESIGN.md)：技术方案、状态机、测量方法
- [PRODUCT_ASSESSMENT.md](PRODUCT_ASSESSMENT.md)：竞品与立项评估
- [Releases](https://github.com/HRLoveFun/relaxation-reminder/releases)：下载与更新日志

## 许可证

[MIT](LICENSE)

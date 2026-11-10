# 休息提醒 · Relaxation Reminder

> 一个会在鼠标旁慢慢变大的休息提醒。到点提醒，真正离开键鼠才开始休息倒计时；轻量常驻、不抢焦点。

[![Release](https://img.shields.io/github/v/release/HRLoveFun/relaxation-reminder?display_name=tag&sort=semver)](https://github.com/HRLoveFun/relaxation-reminder/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078D4)](#系统要求)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

**关键词：** 休息提醒 · 久坐提醒 · 护眼提醒 · 定时休息 · 轻量常驻 · 托盘工具 · 开机自启 · 自定义形象

[安装](#安装) · [功能](#功能) · [使用说明](#使用说明) · [配置](#配置) · [常见问题](#常见问题)

## 安装

**默认方式：AI 智能体安装。** 把下面这段提示词交给 Claude Code、Codex CLI 等能执行本地命令的智能体即可：

```text
请在这台电脑上安装/更新并验证「休息提醒（Relaxation Reminder）」。先读取安装说明并严格照做：
https://raw.githubusercontent.com/HRLoveFun/relaxation-reminder/main/INSTALL.md
只从官方仓库下载，不关闭安全软件，不请求管理员权限；完成后按说明报告安装版本、路径和 SHA-256。
```

提示词故意保持简短：下载地址、校验方式、静默安装参数、验收标准和失败处理都在 [INSTALL.md](INSTALL.md) 里，由智能体自行读取。你只需要在最后确认托盘图标出现。

- **前提**：64 位 Windows；智能体能执行本地命令并能访问 GitHub。
- **安全约束**：安装包未做代码签名。智能体只会从官方 Release 下载、校验 SHA-256，不会关闭安全软件或请求管理员权限。
- **智能体无法联网？** 使用下面的手动安装。

### 手动安装（备用）

1. 从 [Releases](https://github.com/HRLoveFun/relaxation-reminder/releases/latest) 下载 `RelaxationReminder-Setup-0.11.25.1.exe`。
2. 双击安装。当前用户安装，无需管理员权限，默认目录 `%LOCALAPPDATA%\Programs\RelaxationReminder`；SmartScreen 提示时选“更多信息”→“仍要运行”。
3. 安装后自动启动并常驻托盘。想立刻看效果：右键托盘图标 → **立即休息**。

默认每 30 分钟提醒一次，休息 5 分钟，延后 5 分钟，空闲 10 秒判定开始休息。

#### 命令行静默安装

```powershell
Start-Process -FilePath '.\RelaxationReminder-Setup-0.11.25.1.exe' `
  -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-' -Wait
Start-Process "$env:LOCALAPPDATA\Programs\RelaxationReminder\RelaxationReminder.exe"
```

静默安装会跳过安装后的自动启动，因此需要第二行。完整步骤、SHA-256 校验与卸载命令见 [INSTALL.md](INSTALL.md)。

## 系统要求

Windows 10 1809 及以上，64 位。无需管理员权限，无需 .NET / Visual C++ 运行库（静态链接 CRT）。

## 为什么需要它

普通通知很容易被忽略：看到了，点掉，继续工作。这个工具换成一种无法忽略的方式：

- **提醒会“长大”**：窗口从鼠标位置出现，60 秒内指数扩大到工作区面积的 30%，持续占据视野。
- **只有离开键鼠才算休息**：空闲满阈值（默认 10 秒）才开始休息倒计时；中途操作则暂停，再次离开后继续。
- **不抢焦点、轻量常驻**：置顶但不抢输入焦点；exe 约 360 KB（含内置音乐），空闲内存约 17 MB，无第三方运行时与联网请求。

## 功能

- **定时提醒**：按间隔弹出，间隔从本次提醒最终消失后开始计算。
- **鼠标位置出现 + 指数扩大**：初始边长为显示器短边的 1/3（随屏幕与 DPI 缩放），60 秒达到鼠标所在显示器工作区的 30%。
- **休息判定**：键鼠空闲满阈值开始倒计时；有输入暂停，再次空闲继续。
- **延后一次**：每次提醒可延后一次，重弹后回到初始尺寸且按钮置灰。
- **手动关闭**：二次确认，并提示下次提醒时间。
- **暂停计时**：托盘可随时暂停/恢复全部计时，窗口保持不变。
- **休息结束音乐**：休息倒计时结束开始播放音乐（默认 60 秒，可在面板调 30–120 秒），期间窗口保持打开；动键鼠即淡出。可换成自己的 WAV。
- **托盘常驻**：设置、立即休息、暂停计时、形象、音乐、开机自启、退出。
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
5. 倒计时结束开始播放音乐（默认 60 秒），窗口保持打开，标题变为“生产队的驴都不敢这么歇！”；动键鼠后音乐淡出、窗口关闭。
6. 音乐播放完或手动关闭后窗口消失，重新等待 30 分钟。

> 延后会让下一次提醒整体顺延。

## 使用说明

### 提醒窗口

| 操作 | 说明 |
|---|---|
| **延后** | 二次确认后关闭窗口，`snooze_duration_minutes`（默认 5 分钟）后重新弹出；每次提醒只能延后一次。 |
| **关闭** | 二次确认后立即结束本次提醒，并重新等待 `reminder_interval_minutes`。 |
| **设置** | 打开参数面板；面板独立置顶，不随提醒窗口消失。 |

空闲满阈值后自动进入休息倒计时，倒计时结束开始播放音乐，窗口在音乐结束后消失。二次确认窗口打开期间，休息倒计时、音乐与窗口扩大暂停；音乐期间不可延后。

### 托盘菜单

| 菜单项 | 作用 |
|---|---|
| **设置** | 打开参数面板（左键双击托盘图标也可）。 |
| **立即休息** | 立即弹出提醒；已在显示时不做处理。会同时恢复计时。 |
| **暂停计时** | 暂停/恢复全部计时（等待间隔、休息倒计时与窗口扩大）。 |
| **选择形象图片** | 选择本地 PNG/JPEG/BMP/GIF 作为提醒形象。 |
| **恢复默认形象** | 删除自定义形象，回到默认方块。 |
| **休息结束音乐** | 勾选/取消休息结束时的音乐，立即生效。 |
| **选择音乐** | 选择本地 WAV（PCM，≤5 MB）作为音乐。 |
| **恢复默认音乐** | 删除自定义音乐，回到内置音乐。 |
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
| 音乐时长 | 30–120 秒（步长 10） | 休息结束后音乐播放时长，默认 60 秒。 |
| 休息结束音乐 | 开 / 关 | 休息结束时是否播放音乐；默认开启。 |
| 恢复默认值 | — | 恢复 30/5/5、60 秒和“昵称”；不改变空闲阈值、开机自启与音乐开关。 |

`idle_threshold_seconds`（空闲阈值，默认 10 秒）只能通过配置文件修改，见[配置](#配置)。

### 自定义形象

托盘右键 → **选择形象图片**，图片复制到 `%APPDATA%\RelaxationReminder\character` 并自动加载；**恢复默认形象** 可还原。单文件 ≤10 MB，单帧 ≤1024×1024，动图 ≤500 帧；缺失或解码失败时回退默认方块。

### 休息结束音乐

休息倒计时归零后开始循环播放音乐，持续 `music_play_seconds`（默认 60 秒，面板可调 30–120 秒）；每遍播放 2 秒后停顿 3 秒，再继续下一遍。期间提醒窗口保持打开、标题显示“生产队的驴都不敢这么歇！”。播放期间动键盘或鼠标，音乐在 1 秒内淡出，窗口在 1 秒后关闭；关闭、延后、暂停计时、休眠或退出时音乐立即停止。关闭音乐时，休息倒计时结束即关闭窗口（旧行为）。

托盘右键 → **选择音乐**，WAV 复制到 `%APPDATA%\RelaxationReminder\sound`；**恢复默认音乐** 可还原。仅支持 PCM WAV（8/16 位、1~2 声道、8~48 kHz），单文件 ≤5 MB；缺失或格式不符时回退内置音乐。内置音乐由程序合成（`tools/make_music.py`），复刻了原提示音前 2 秒的铃声，不包含第三方音频。

### 开机自启与卸载

- 自启默认开启，可在托盘关闭；每次启动按当前 exe 路径重写 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\RelaxationReminder`。
- 从“设置 → 应用 → 已安装的应用”卸载；默认保留 `%APPDATA%\RelaxationReminder`，彻底清除需手动删除。
- 覆盖安装会先请求正在运行的实例退出。

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
| `music_enabled` | 1 | 是否播放休息结束音乐，0/1 |
| `music_play_seconds` | 60 | 音乐播放时长，30–120；面板限制 30–120 步长 10 |
| `music_file` | 空 | 自定义音乐文件名；仅文件名，不含路径，须为 `.wav` |

非法数值回退默认值；面板档位外的历史值会在打开面板时就近归档并保存。

## 从源码构建

环境：64 位 Windows、Visual Studio 2019（v142）与 Windows SDK，安装“使用 C++ 的桌面开发”；打包需 [Inno Setup 6](https://jrsoftware.org/isdl.php)。

```bat
build.bat
build.bat test
build.bat installer
```

产物：`build\RelaxationReminder.exe`（单文件、静态 CRT）与 `dist\RelaxationReminder-Setup-0.11.25.1.exe`。也可用 CMake：

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
这就是默认方式：把[安装](#安装)里的简短提示词交给能执行本地命令的智能体即可。

**支持 macOS / iOS 吗？**
当前只提供 Windows 桌面端，macOS 与 iOS 在规划中。

## 项目文档

- [INSTALL.md](INSTALL.md)：完整安装说明（AI 智能体与手动安装）
- [AGENTS.md](AGENTS.md)：需求与功能定义
- [TECH_DESIGN.md](TECH_DESIGN.md)：技术方案、状态机、测量方法
- [PRODUCT_ASSESSMENT.md](PRODUCT_ASSESSMENT.md)：竞品与立项评估
- [Releases](https://github.com/HRLoveFun/relaxation-reminder/releases)：下载与更新日志

## 许可证

[MIT](LICENSE)

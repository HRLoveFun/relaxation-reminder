# 安装说明

本页是「休息提醒（Relaxation Reminder）」的完整安装说明，供 AI 智能体和手动安装共用。

- 智能体：按[给 AI 智能体的说明](#给-ai-智能体的说明)执行。
- 手动安装：见[手动安装](#手动安装备用)。

---

## 给 AI 智能体的说明

### 目标

在这台 Windows x64 电脑上安装「休息提醒（Relaxation Reminder）」并验证结果。这是一个常驻托盘的休息提醒工具，安装后默认每 30 分钟提醒一次。全程使用命令行完成，不要让用户手动点击安装向导。

### 安全约束（必须遵守）

1. 只从官方仓库 <https://github.com/HRLoveFun/relaxation-reminder> 的 Releases 下载，不使用任何第三方镜像或转载地址。
2. 安装包未做代码签名。不要关闭 Windows Defender、SmartScreen，不要添加杀毒排除项，也不要用任何方式绕过系统安全提示。
3. 以当前用户权限安装，不要请求管理员权限。如果某一步必须提权，停下来说明原因并等待用户确认。
4. 下载失败、哈希不一致或安装退出码非 0 时停止，把错误原文告诉用户，不要自行换源重试。
5. 不要修改用户的配置文件或系统设置（程序自身默认写入的开机自启项除外）。

### 执行步骤

1. 确认系统为 64 位 Windows：

   ```powershell
   [System.Environment]::Is64BitOperatingSystem
   ```

   不满足就停止并告诉用户。

2. 读取最新 Release 并找到安装包：

   ```powershell
   $release = Invoke-RestMethod -Uri 'https://api.github.com/repos/HRLoveFun/relaxation-reminder/releases/latest' -Headers @{ 'User-Agent' = 'RelaxationReminder-Installer' }
   $asset = $release.assets | Where-Object { $_.name -like 'RelaxationReminder-Setup-*.exe' } | Select-Object -First 1
   ```

   如果找不到安装包，停止并报告。

3. 下载到临时目录，并移除“来自 Internet”标记：

   ```powershell
   $dir = Join-Path $env:TEMP 'RelaxationReminder-Install'
   New-Item -ItemType Directory -Force -Path $dir | Out-Null
   $setup = Join-Path $dir $asset.name
   curl.exe -L --fail --retry 3 -o $setup $asset.browser_download_url
   Unblock-File -Path $setup
   ```

4. 计算 SHA-256，并在 `$release.body` 中查找对照值：

   ```powershell
   (Get-FileHash $setup -Algorithm SHA256).Hash.ToLower()
   ```

   如果 Release 说明没有给出哈希，记录计算结果并在最终报告中列出；不一致则停止。

5. 静默安装（Inno Setup 参数；安装器会先请求已运行的旧版本退出，配置与自定义形象保留）：

   ```powershell
   $log = Join-Path $dir 'install.log'
   $p = Start-Process -FilePath $setup -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',('/LOG="{0}"' -f $log)) -PassThru -Wait
   ```

   退出码 0 或 3010 视为成功；其他值请读取 `$log` 后报告。

6. 启动程序（静默安装不会自动启动）：

   ```powershell
   $exe = Join-Path $env:LOCALAPPDATA 'Programs\RelaxationReminder\RelaxationReminder.exe'
   Start-Process $exe
   ```

7. 验证：

   - `Get-Process RelaxationReminder` 存在；
   - `Test-Path (Join-Path $env:APPDATA 'RelaxationReminder\config.ini')` 为 `True`；
   - `(Get-Item $exe).VersionInfo.FileVersion` 与 Release 版本一致；
   - 托盘图标无法用脚本可靠确认，请让用户看一下右下角托盘。

8. 最终报告：安装版本、安装路径、配置路径、下载文件的 SHA-256、安装日志路径，以及如何打开设置（左键双击托盘图标）和卸载方式。

如果安装失败且提示文件被占用，请提示用户通过托盘菜单「退出」后重试，不要强制结束进程。

`Get-FileHash` 不可用时可用 `certutil -hashfile <文件> SHA256` 代替。

---

## 手动安装（备用）

1. 从 [Releases](https://github.com/HRLoveFun/relaxation-reminder/releases/latest) 下载 `RelaxationReminder-Setup-0.11.25.0.exe`。
2. 双击安装。当前用户安装，无需管理员权限，默认目录 `%LOCALAPPDATA%\Programs\RelaxationReminder`；SmartScreen 提示时选“更多信息”→“仍要运行”。
3. 安装后自动启动并常驻托盘。想立刻看效果：右键托盘图标 → **立即休息**。

默认每 30 分钟提醒一次，休息 5 分钟，延后 5 分钟，空闲 10 秒判定开始休息。

### 命令行静默安装

```powershell
Start-Process -FilePath '.\RelaxationReminder-Setup-0.11.25.0.exe' `
  -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-' -Wait
Start-Process "$env:LOCALAPPDATA\Programs\RelaxationReminder\RelaxationReminder.exe"
```

静默安装会跳过安装后的自动启动，因此需要第二行。

### 卸载

```powershell
Start-Process "$env:LOCALAPPDATA\Programs\RelaxationReminder\unins000.exe" `
  -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART' -Wait
```

卸载默认保留 `%APPDATA%\RelaxationReminder` 中的配置与自定义形象。

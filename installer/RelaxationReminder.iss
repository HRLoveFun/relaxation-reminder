; Relaxation Reminder - per-user installer.
; Build: "C:\Users\<user>\AppData\Local\Programs\Inno Setup 6\ISCC.exe" installer\RelaxationReminder.iss
; Output: dist\RelaxationReminder-Setup-<version>.exe

#define MyAppName "休息提醒"
#define MyAppVersion "0.11.25.0"
#define MyAppPublisher "HRLoveFun"
#define MyAppExeName "RelaxationReminder.exe"

[Setup]
; Keep this GUID stable: it identifies the product across upgrades.
AppId={{07BEC084-9BBA-4BF1-97D8-526F03784FA3}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\Programs\RelaxationReminder
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir=..\dist
OutputBaseFilename=RelaxationReminder-Setup-{#MyAppVersion}
SetupIconFile=..\src\resources\app.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
CloseApplicationsFilter={#MyAppExeName}
RestartApplications=no
UsePreviousAppDir=yes
ShowLanguageDialog=no
SetupLogging=yes

[Languages]
Name: "chinesesimplified"; MessagesFile: "ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\build\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; The app manages this value itself (default on, user can turn it off).
; Delete it on uninstall, but never (re)create it during install/upgrade.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "RelaxationReminder"; Flags: uninsdeletevalue

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

[Code]
const
  WM_CLOSE = $0010;

procedure StopRunningInstance;
var
  Hwnd: HWND;
  I: Integer;
begin
  Hwnd := FindWindowByClassName('RelaxationReminderHiddenWindow');
  if Hwnd <> 0 then
  begin
    SendMessage(Hwnd, WM_CLOSE, 0, 0);
    for I := 0 to 30 do
    begin
      Sleep(100);
      if FindWindowByClassName('RelaxationReminderHiddenWindow') = 0 then
        Break;
    end;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  StopRunningInstance;
  Result := '';
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    StopRunningInstance;
end;

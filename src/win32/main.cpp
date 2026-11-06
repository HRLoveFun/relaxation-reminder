#define _WIN32_WINNT 0x0A00

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <memory>
#include <string>
#include <vector>

#include "core/config.h"
#include "core/state_machine.h"
#include "win32/reminder_window.h"
#include "win32/settings_window.h"
#include "win32/splash_window.h"

namespace {

constexpr wchar_t kWindowClass[] = L"RelaxationReminderHiddenWindow";
constexpr wchar_t kAppName[] = L"休息提醒";
constexpr wchar_t kMutexName[] = L"Local\\RelaxationReminder";
constexpr wchar_t kRunKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"RelaxationReminder";

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kShowSettingsMessage = WM_APP + 2;
constexpr UINT_PTR kTickTimer = 1;
constexpr UINT kTickIntervalMs = 50;
constexpr UINT kTrayIconId = 1;

constexpr UINT kMenuSettings = 1001;
constexpr UINT kMenuRestNow = 1002;
constexpr UINT kMenuAutostart = 1003;
constexpr UINT kMenuExit = 1004;
constexpr UINT kMenuChooseImage = 1005;
constexpr UINT kMenuResetImage = 1006;

HWND g_hwnd = nullptr;
NOTIFYICONDATAW g_tray{};
UINT g_taskbar_created_message = 0;
rr::Config g_config;
bool g_manage_autostart = true;
rr::StateMachine g_machine;
std::unique_ptr<rr::ReminderWindow> g_reminder;
std::unique_ptr<rr::SettingsWindow> g_settings;
std::unique_ptr<rr::SplashWindow> g_splash;
std::wstring g_last_tip;
uint64_t g_last_tip_update = 0;

std::wstring AppDataDir() {
    wchar_t buffer[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                                nullptr, SHGFP_TYPE_CURRENT, buffer))) {
        return L".";
    }
    std::wstring dir = std::wstring(buffer) + L"\\RelaxationReminder";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::wstring ConfigPath() {
    return AppDataDir() + L"\\config.ini";
}

std::wstring CharacterDir() {
    std::wstring dir = AppDataDir() + L"\\character";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::wstring FileNamePart(const std::wstring& path) {
    const size_t separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? path : path.substr(separator + 1);
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), length);
    result.resize(static_cast<size_t>(length - 1));
    return result;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0,
                                     nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), length,
                        nullptr, nullptr);
    result.resize(static_cast<size_t>(length - 1));
    return result;
}

bool SaveConfig() {
    const std::wstring path = ConfigPath();
    const std::wstring temp = path + L".tmp";
    const std::wstring image = Utf8ToWide(g_config.character_image_file);

    wchar_t body[2048] = {};
    swprintf_s(body, _countof(body),
               L"[general]\r\n"
               L"reminder_interval_minutes=%u\r\n"
               L"rest_duration_minutes=%u\r\n"
               L"snooze_duration_minutes=%u\r\n"
               L"idle_threshold_seconds=%u\r\n"
               L"autostart=%u\r\n"
               L"user_name=%ls\r\n"
               L"character_image_file=%ls\r\n",
               g_config.reminder_interval_minutes,
               g_config.rest_duration_minutes,
               g_config.snooze_duration_minutes,
               g_config.idle_threshold_seconds,
               g_config.autostart ? 1u : 0u, g_config.user_name.c_str(),
               image.c_str());

    HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    const WORD bom = 0xFEFF;
    DWORD written = 0;
    bool ok = WriteFile(file, &bom, sizeof(bom), &written, nullptr) != FALSE;
    if (ok) {
        const DWORD bytes =
            static_cast<DWORD>(wcslen(body) * sizeof(wchar_t));
        ok = WriteFile(file, body, bytes, &written, nullptr) != FALSE;
    }
    CloseHandle(file);

    if (!ok) {
        DeleteFileW(temp.c_str());
        return false;
    }
    return MoveFileExW(temp.c_str(), path.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

void LoadConfig() {
    const std::wstring path = ConfigPath();

    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        g_config = rr::Config{};
        SaveConfig();
        return;
    }

    rr::Config loaded;
    loaded.reminder_interval_minutes = GetPrivateProfileIntW(
        L"general", L"reminder_interval_minutes",
        rr::kDefaultReminderIntervalMinutes, path.c_str());
    loaded.rest_duration_minutes = GetPrivateProfileIntW(
        L"general", L"rest_duration_minutes",
        rr::kDefaultRestDurationMinutes, path.c_str());
    loaded.snooze_duration_minutes = GetPrivateProfileIntW(
        L"general", L"snooze_duration_minutes",
        rr::kDefaultSnoozeDurationMinutes, path.c_str());
    loaded.idle_threshold_seconds = GetPrivateProfileIntW(
        L"general", L"idle_threshold_seconds",
        rr::kDefaultIdleThresholdSeconds, path.c_str());
    loaded.autostart =
        GetPrivateProfileIntW(L"general", L"autostart", 1, path.c_str()) != 0;

    wchar_t user_name[64] = {};
    GetPrivateProfileStringW(L"general", L"user_name", rr::kDefaultUserName,
                             user_name, _countof(user_name), path.c_str());
    loaded.user_name = user_name;

    wchar_t image[300] = {};
    GetPrivateProfileStringW(L"general", L"character_image_file", L"", image,
                             _countof(image), path.c_str());
    loaded.character_image_file = WideToUtf8(image);

    g_config = rr::Sanitize(loaded);
}

bool InstallCharacterImage(const std::wstring& source,
                           std::wstring* installed_name,
                           std::wstring* error) {
    const auto fail = [error](const wchar_t* message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    {
        rr::CharacterImage probe;
        std::wstring load_error;
        if (!probe.Load(source, &load_error)) {
            if (error != nullptr) {
                *error = load_error;
            }
            return false;
        }
    }

    std::wstring name = FileNamePart(source);
    if (name.empty() || name.size() > 200) {
        return fail(L"文件名无效或过长。");
    }
    if (name.find_first_of(L"<>:\"|?*") != std::wstring::npos) {
        return fail(L"文件名包含无效字符。");
    }

    const std::wstring directory = CharacterDir();
    std::wstring destination = directory + L"\\" + name;
    const bool source_is_installed =
        _wcsicmp(source.c_str(), destination.c_str()) == 0;

    if (!source_is_installed) {
        if (GetFileAttributesW(destination.c_str()) != INVALID_FILE_ATTRIBUTES) {
            const size_t dot = name.find_last_of(L'.');
            const std::wstring stem =
                dot == std::wstring::npos ? name : name.substr(0, dot);
            const std::wstring extension =
                dot == std::wstring::npos ? L"" : name.substr(dot);
            bool found = false;
            for (int index = 2; index < 100; ++index) {
                std::wstring candidate =
                    stem + L"_" + std::to_wstring(index) + extension;
                std::wstring candidate_path = directory + L"\\" + candidate;
                if (GetFileAttributesW(candidate_path.c_str()) ==
                    INVALID_FILE_ATTRIBUTES) {
                    name = candidate;
                    destination = candidate_path;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return fail(L"形象目录中同名文件过多。");
            }
        }

        const std::wstring temp = destination + L".tmp";
        DeleteFileW(temp.c_str());
        if (!CopyFileW(source.c_str(), temp.c_str(), FALSE)) {
            return fail(L"复制图片失败。");
        }
        if (!MoveFileExW(temp.c_str(), destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            DeleteFileW(temp.c_str());
            return fail(L"保存图片失败。");
        }
    }

    if (g_reminder) {
        std::wstring load_error;
        if (!g_reminder->LoadCharacter(destination, &load_error)) {
            if (!source_is_installed) {
                DeleteFileW(destination.c_str());
            }
            if (error != nullptr) {
                *error = load_error;
            }
            return false;
        }
    }

    const std::wstring old_name = Utf8ToWide(g_config.character_image_file);
    g_config.character_image_file = WideToUtf8(name);
    SaveConfig();

    if (!old_name.empty() &&
        _wcsicmp(old_name.c_str(), name.c_str()) != 0) {
        DeleteFileW((directory + L"\\" + old_name).c_str());
    }

    if (installed_name != nullptr) {
        *installed_name = name;
    }
    return true;
}

void LoadConfiguredCharacter() {
    if (!g_reminder) {
        return;
    }
    if (g_config.character_image_file.empty()) {
        g_reminder->ClearCharacter();
        return;
    }

    const std::wstring path =
        CharacterDir() + L"\\" + Utf8ToWide(g_config.character_image_file);
    std::wstring error;
    if (!g_reminder->LoadCharacter(path, &error)) {
        g_config.character_image_file.clear();
        SaveConfig();
    }
}

void ChooseCharacterImage() {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner =
        (g_reminder && g_reminder->visible()) ? g_reminder->hwnd() : g_hwnd;
    dialog.lpstrFilter =
        L"图片 (*.png;*.jpg;*.jpeg;*.bmp;*.gif)\0"
        L"*.png;*.jpg;*.jpeg;*.bmp;*.gif\0"
        L"所有文件 (*.*)\0*.*\0\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = _countof(file);
    dialog.lpstrTitle = L"选择形象图片";
    dialog.Flags =
        OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) {
        return;
    }

    std::wstring error;
    if (!InstallCharacterImage(file, nullptr, &error)) {
        MessageBoxW(g_hwnd, error.c_str(), L"无法使用该图片",
                    MB_OK | MB_ICONWARNING);
    }
}

void ResetCharacterImage() {
    const std::wstring old_name = Utf8ToWide(g_config.character_image_file);
    g_config.character_image_file.clear();
    SaveConfig();

    if (g_reminder) {
        g_reminder->ClearCharacter();
    }
    if (!old_name.empty()) {
        DeleteFileW((CharacterDir() + L"\\" + old_name).c_str());
    }
}

bool IsAutostartEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key) !=
        ERROR_SUCCESS) {
        return false;
    }
    wchar_t value[1024] = {};
    DWORD size = sizeof(value);
    DWORD type = 0;
    const LSTATUS status =
        RegQueryValueExW(key, kRunValue, nullptr, &type,
                         reinterpret_cast<BYTE*>(value), &size);
    RegCloseKey(key);
    return status == ERROR_SUCCESS && type == REG_SZ;
}

bool SetAutostart(bool enable) {
    HKEY key = nullptr;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, &disposition) !=
        ERROR_SUCCESS) {
        return false;
    }

    LSTATUS status = ERROR_SUCCESS;
    if (enable) {
        wchar_t exe[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exe, _countof(exe));
        std::wstring command = L"\"" + std::wstring(exe) + L"\"";
        status = RegSetValueExW(
            key, kRunValue, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        status = RegDeleteValueW(key, kRunValue);
        if (status == ERROR_FILE_NOT_FOUND) {
            status = ERROR_SUCCESS;
        }
    }

    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

uint32_t IdleMilliseconds() {
    LASTINPUTINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetLastInputInfo(&info)) {
        return 0;
    }
    const DWORD now = static_cast<DWORD>(GetTickCount64());
    return now - info.dwTime;
}

std::wstring FormatClock(uint32_t seconds) {
    wchar_t buffer[16] = {};
    swprintf_s(buffer, L"%02u:%02u", seconds / 60, seconds % 60);
    return buffer;
}

void UpdateTrayTip() {
    std::wstring tip = kAppName;

    switch (g_machine.state()) {
        case rr::State::WaitInterval:
            tip += L" - 下次 ";
            tip += FormatClock(g_machine.interval_remaining_seconds());
            break;
        case rr::State::SnoozeWait:
            tip += L" - 延后中 ";
            tip += FormatClock(g_machine.interval_remaining_seconds());
            break;
        case rr::State::ShowActive:
            tip += L" - 该休息了";
            break;
        case rr::State::RestRunning:
            tip += L" - 休息中 ";
            tip += FormatClock(g_machine.rest_remaining_seconds());
            break;
        case rr::State::RestPaused:
            tip += L" - 休息暂停，离开键鼠继续";
            break;
    }

    if (tip == g_last_tip) {
        return;
    }
    g_last_tip = tip;

    wcsncpy_s(g_tray.szTip, _countof(g_tray.szTip), tip.c_str(), _TRUNCATE);
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_tray);
}

void ShowReminderWindow() {
    if (!g_reminder) {
        return;
    }
    const uint64_t now = GetTickCount64();
    g_reminder->ShowAtCursor(now, g_machine.snooze_used());
    g_reminder->Update(now, g_machine);
    UpdateTrayTip();
}

bool ConfirmSnoozeReminder() {
    wchar_t text[512] = {};
    swprintf_s(text, _countof(text),
               L"确定要延后本次休息提醒吗？\n\n"
               L"延后后：本次提醒立即消失，程序会在 %u 分钟后再次提醒你。\n"
               L"每次提醒只能延后一次，重新弹出的窗口不能再延后。",
               g_config.snooze_duration_minutes);

    HWND owner =
        (g_reminder && g_reminder->visible()) ? g_reminder->hwnd() : g_hwnd;
    const int answer = MessageBoxW(
        owner, text, L"确认延后提醒",
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_TOPMOST);
    return answer == IDYES;
}

void HandleSnooze() {
    if (!g_machine.IsVisible() || g_machine.snooze_used()) {
        return;
    }
    if (!ConfirmSnoozeReminder()) {
        return;
    }
    if (!g_machine.TrySnooze(GetTickCount64())) {
        return;
    }
    if (g_reminder) {
        g_reminder->Hide();
    }
    UpdateTrayTip();
}

bool ConfirmCloseReminder() {
    wchar_t text[512] = {};
    swprintf_s(text, _countof(text),
               L"确定要关闭本次休息提醒吗？\n\n"
               L"建议现在离开键鼠，休息 %u 分钟。\n\n"
               L"关闭后：本次提醒立即结束，程序会在 %u 分钟后再次提醒你。",
               g_config.rest_duration_minutes,
               g_config.reminder_interval_minutes);

    HWND owner =
        (g_reminder && g_reminder->visible()) ? g_reminder->hwnd() : g_hwnd;
    const int answer = MessageBoxW(
        owner, text, L"确认关闭提醒",
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_TOPMOST);
    return answer == IDYES;
}

void HandleCloseReminder() {
    if (!g_machine.IsVisible()) {
        return;
    }
    if (!ConfirmCloseReminder()) {
        return;
    }
    g_machine.CloseReminder(GetTickCount64());
    if (g_reminder) {
        g_reminder->Hide();
    }
    UpdateTrayTip();
}

void ApplySettings(const rr::Config& config) {
    // The settings panel no longer edits autostart; it is controlled from the
    // tray menu, so keep the current value and never let a panel edit revert it.
    const bool autostart = g_config.autostart;
    g_config = config;
    g_config.autostart = autostart;
    SaveConfig();
    g_machine.Configure(config.reminder_interval_minutes,
                        config.rest_duration_minutes,
                        config.snooze_duration_minutes,
                        config.idle_threshold_seconds);
    if (g_reminder) {
        g_reminder->SetUserName(g_config.user_name);
    }
    UpdateTrayTip();
}

void ShowSettingsPanel() {
    if (!g_settings) {
        return;
    }
    if (g_splash) {
        g_splash->Hide();
    }
    g_settings->Show(g_config, ApplySettings);
}

void ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    AppendMenuW(menu, MF_STRING, kMenuSettings, L"设置(&S)");
    AppendMenuW(menu, MF_STRING, kMenuRestNow, L"立即休息(&N)");
    AppendMenuW(menu, MF_STRING, kMenuChooseImage, L"选择形象图片(&I)…");
    AppendMenuW(menu,
                MF_STRING | (g_config.character_image_file.empty() ? MF_GRAYED
                                                                  : MF_ENABLED),
                kMenuResetImage, L"恢复默认形象(&R)");
    AppendMenuW(menu,
                MF_STRING | (IsAutostartEnabled() ? MF_CHECKED : MF_UNCHECKED),
                kMenuAutostart, L"开机自启(&A)");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"退出(&X)");

    POINT point = {};
    GetCursorPos(&point);
    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                   point.x, point.y, 0, g_hwnd, nullptr);
    DestroyMenu(menu);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
}

void AddTrayIcon() {
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = g_hwnd;
    g_tray.uID = kTrayIconId;
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_tray.uCallbackMessage = kTrayMessage;
    g_tray.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
    if (g_tray.hIcon == nullptr) {
        g_tray.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    wcsncpy_s(g_tray.szTip, _countof(g_tray.szTip), kAppName, _TRUNCATE);
    g_last_tip.clear();
    Shell_NotifyIconW(NIM_ADD, &g_tray);
}

void RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_tray);
}

void Tick() {
    const uint64_t now = GetTickCount64();
    const rr::TickResult result = g_machine.Tick(now, IdleMilliseconds());

    if (result.show_reminder) {
        ShowReminderWindow();
    }
    if (result.final_dismiss || result.hide_reminder) {
        if (g_reminder) {
            g_reminder->Hide();
        }
        UpdateTrayTip();
    }
    if (result.entered_rest || result.paused_rest || result.resumed_rest) {
        UpdateTrayTip();
    }

    if (g_reminder && g_reminder->visible() && g_machine.IsVisible()) {
        g_reminder->Update(now, g_machine);
    }

    if (now - g_last_tip_update >= 1000) {
        g_last_tip_update = now;
        UpdateTrayTip();
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam,
                         LPARAM lparam) {
    if (g_taskbar_created_message != 0 &&
        message == g_taskbar_created_message) {
        AddTrayIcon();
        UpdateTrayTip();
        return 0;
    }

    switch (message) {
        case WM_TIMER:
            if (wparam == kTickTimer) {
                Tick();
            }
            return 0;

        case kTrayMessage:
            if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU) {
                ShowTrayMenu();
            } else if (lparam == WM_LBUTTONDBLCLK) {
                ShowSettingsPanel();
            }
            return 0;

        case kShowSettingsMessage:
            ShowSettingsPanel();
            return 0;

        case rr::ReminderWindow::kSnoozeMessage:
            HandleSnooze();
            return 0;

        case rr::ReminderWindow::kCloseMessage:
            HandleCloseReminder();
            return 0;

        case rr::ReminderWindow::kSettingsMessage:
            ShowSettingsPanel();
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case kMenuSettings:
                    ShowSettingsPanel();
                    return 0;
                case kMenuRestNow:
                    if (!g_machine.IsVisible()) {
                        if (g_settings && g_settings->visible()) {
                            g_settings->Hide();
                        }
                        if (g_splash) {
                            g_splash->Hide();
                        }
                        g_machine.ForceShow(GetTickCount64());
                        ShowReminderWindow();
                    }
                    return 0;
                case kMenuChooseImage:
                    ChooseCharacterImage();
                    return 0;
                case kMenuResetImage:
                    ResetCharacterImage();
                    return 0;
                case kMenuAutostart: {
                    const bool enable = !IsAutostartEnabled();
                    if (SetAutostart(enable)) {
                        g_config.autostart = enable;
                        SaveConfig();
                    }
                    UpdateTrayTip();
                    return 0;
                }
                case kMenuExit:
                    DestroyWindow(hwnd);
                    return 0;
                default:
                    break;
            }
            break;

        case WM_POWERBROADCAST:
            if (wparam == PBT_APMSUSPEND) {
                g_machine.OnSuspend(GetTickCount64());
            } else if (wparam == PBT_APMRESUMEAUTOMATIC) {
                g_machine.OnResume(GetTickCount64());
                Tick();
            }
            return TRUE;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, kTickTimer);
            RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    const bool already_running =
        mutex != nullptr && GetLastError() == ERROR_ALREADY_EXISTS;
    if (already_running) {
        HWND existing = FindWindowW(kWindowClass, nullptr);
        if (existing) {
            PostMessageW(existing, kShowSettingsMessage, 0, 0);
        }
        CloseHandle(mutex);
        return 0;
    }

    LoadConfig();

    // Development/testing switch: avoid touching the Run key.
    g_manage_autostart =
        wcsstr(GetCommandLineW(), L"--no-autostart") == nullptr;
    if (g_manage_autostart) {
        if (g_config.autostart) {
            // Rewrite the value on every start so the stored path follows the
            // executable if the app was moved or reinstalled.
            SetAutostart(true);
        } else if (IsAutostartEnabled()) {
            SetAutostart(false);
        }
    }

    Gdiplus::GdiplusStartupInput gdiplus_input;
    ULONG_PTR gdiplus_token = 0;
    if (Gdiplus::GdiplusStartup(&gdiplus_token, &gdiplus_input, nullptr) !=
        Gdiplus::Ok) {
        if (mutex) {
            ReleaseMutex(mutex);
            CloseHandle(mutex);
        }
        return 1;
    }

    g_taskbar_created_message = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = WndProc;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    window_class.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&window_class)) {
        Gdiplus::GdiplusShutdown(gdiplus_token);
        if (mutex) {
            ReleaseMutex(mutex);
            CloseHandle(mutex);
        }
        return 1;
    }

    g_hwnd = CreateWindowExW(0, kWindowClass, kAppName, WS_OVERLAPPED, 0, 0, 0,
                             0, nullptr, nullptr, instance, nullptr);
    if (!g_hwnd) {
        Gdiplus::GdiplusShutdown(gdiplus_token);
        if (mutex) {
            ReleaseMutex(mutex);
            CloseHandle(mutex);
        }
        return 1;
    }

    g_reminder = std::make_unique<rr::ReminderWindow>();
    if (!g_reminder->Create(instance, g_hwnd)) {
        g_reminder.reset();
        DestroyWindow(g_hwnd);
        Gdiplus::GdiplusShutdown(gdiplus_token);
        if (mutex) {
            ReleaseMutex(mutex);
            CloseHandle(mutex);
        }
        return 1;
    }
    g_reminder->SetUserName(g_config.user_name);
    LoadConfiguredCharacter();

    g_settings = std::make_unique<rr::SettingsWindow>();
    if (!g_settings->Create(instance, g_hwnd)) {
        g_settings.reset();
    }

    g_splash = std::make_unique<rr::SplashWindow>();
    if (!g_splash->Create(instance, g_hwnd)) {
        g_splash.reset();
    }

    AddTrayIcon();

    g_machine.Configure(g_config.reminder_interval_minutes,
                        g_config.rest_duration_minutes,
                        g_config.snooze_duration_minutes,
                        g_config.idle_threshold_seconds);
    g_machine.Start(GetTickCount64());
    UpdateTrayTip();

    bool reminder_forced = false;
#ifdef RR_TEST_MODE
    if (wcsstr(GetCommandLineW(), L"--remind-now") != nullptr) {
        g_machine.ForceShow(GetTickCount64());
        ShowReminderWindow();
        reminder_forced = true;
    }
#endif
    if (!reminder_forced && g_splash) {
        g_splash->Show(g_config, rr::SplashWindow::kDefaultDurationMs);
    }

    SetTimer(g_hwnd, kTickTimer, kTickIntervalMs, nullptr);

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (g_settings && g_settings->visible() &&
            IsDialogMessageW(g_settings->hwnd(), &message)) {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    RemoveTrayIcon();
    g_settings.reset();
    g_splash.reset();
    g_reminder.reset();
    Gdiplus::GdiplusShutdown(gdiplus_token);
    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }
    return 0;
}
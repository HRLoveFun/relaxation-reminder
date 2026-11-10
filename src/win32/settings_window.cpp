#include "settings_window.h"

#include <commctrl.h>
#include <uxtheme.h>

#include <cstdint>
#include <string>
#include <utility>

#include "win32/ui_theme.h"

namespace rr {
namespace {

constexpr wchar_t kWindowClass[] = L"RelaxationReminderSettings";
constexpr UINT kNameEditId = 3001;
constexpr UINT kIntervalSliderId = 3002;
constexpr UINT kRestSliderId = 3003;
constexpr UINT kSnoozeSliderId = 3004;
constexpr UINT kMusicCheckId = 3005;
constexpr UINT kResetButtonId = 3006;
constexpr UINT kMusicPlaySliderId = 3007;
constexpr UINT kCloseButtonId = 3008;

constexpr int kClientWidth = 470;
constexpr int kClientHeight = 300;
constexpr int kLabelX = 20;
constexpr int kControlX = 118;
constexpr int kSliderWidth = 240;
constexpr int kValueX = 368;
constexpr int kFirstRowY = 16;
constexpr int kRowStep = 38;
constexpr int kTextHeight = 30;
constexpr int kEditHeight = 24;
constexpr int kMusicRowY = 206;
constexpr int kButtonTop = 252;

constexpr uint32_t kIntervalValues[] = {20, 30, 40, 50, 60};
constexpr uint32_t kRestValues[] = {5, 10, 15};
constexpr uint32_t kSnoozeValues[] = {5, 10, 15};
constexpr uint32_t kMusicPlayValues[] = {30, 40, 50, 60, 70,
                                         80, 90, 100, 110, 120};

int NearestIndex(const uint32_t* values, int count, uint32_t value) {
    int best = 0;
    uint32_t best_delta = UINT32_MAX;
    for (int index = 0; index < count; ++index) {
        const uint32_t delta =
            values[index] > value ? values[index] - value : value - values[index];
        if (delta < best_delta) {
            best_delta = delta;
            best = index;
        }
    }
    return best;
}

bool SameConfig(const Config& left, const Config& right) {
    return left.reminder_interval_minutes == right.reminder_interval_minutes &&
           left.rest_duration_minutes == right.rest_duration_minutes &&
           left.snooze_duration_minutes == right.snooze_duration_minutes &&
           left.idle_threshold_seconds == right.idle_threshold_seconds &&
           left.autostart == right.autostart &&
           left.user_name == right.user_name &&
           left.character_image_file == right.character_image_file &&
           left.music_enabled == right.music_enabled &&
           left.music_play_seconds == right.music_play_seconds;
}

}  // namespace

SettingsWindow::~SettingsWindow() {
    Destroy();
}

bool SettingsWindow::Create(HINSTANCE instance, HWND owner) {
    instance_ = instance;
    owner_ = owner;
    dpi_ = owner != nullptr ? GetDpiForWindow(owner) : 96;
    if (dpi_ == 0) {
        dpi_ = 96;
    }

    INITCOMMONCONTROLSEX common_controls = {};
    common_controls.dwSize = sizeof(common_controls);
    common_controls.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&common_controls);

    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = WndProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    // Paint the background explicitly so the panel keeps the same light look
    // regardless of the system dark-mode setting.
    background_ = CreateSolidBrush(RGB(240, 240, 240));
    window_class.hbrBackground = nullptr;
    window_class.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&window_class) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    const DWORD ex_style =
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT;
    const int scale = static_cast<int>(dpi_);
    RECT rect = {0, 0, MulDiv(kClientWidth, scale, 96),
                 MulDiv(kClientHeight, scale, 96)};
    AdjustWindowRectEx(&rect, style, FALSE, ex_style);

    hwnd_ = CreateWindowExW(
        ex_style, kWindowClass, L"参数设置", style, CW_USEDEFAULT,
        CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, owner,
        nullptr, instance, this);
    if (hwnd_ == nullptr) {
        return false;
    }

    CreateControls();
    UpdateFonts();
    Layout();
    return true;
}

void SettingsWindow::Destroy() {
    if (hwnd_ != nullptr && IsWindow(hwnd_) != FALSE) {
        DestroyWindow(hwnd_);
    }
    hwnd_ = nullptr;
    name_edit_ = nullptr;
    music_checkbox_ = nullptr;
    for (Slider& slider : sliders_) {
        slider.track = nullptr;
    }
    reset_button_ = nullptr;
    close_button_ = nullptr;
    visible_ = false;

    if (font_ != nullptr) {
        DeleteObject(font_);
        font_ = nullptr;
    }
    if (background_ != nullptr) {
        DeleteObject(background_);
        background_ = nullptr;
    }
}

void SettingsWindow::Show(const Config& current, SaveHandler on_save) {
    if (hwnd_ == nullptr) {
        return;
    }
    base_config_ = current;
    on_save_ = std::move(on_save);

    // Values stored before the slider ranges existed may not match an option.
    // Snap them to the nearest allowed value once, so the panel always shows
    // exactly what the configuration contains.
    bool normalized = false;
    for (const Slider& slider : sliders_) {
        const uint32_t value = base_config_.*(slider.member);
        const uint32_t snapped =
            slider.values[NearestIndex(slider.values, slider.count, value)];
        if (snapped != value) {
            base_config_.*(slider.member) = snapped;
            normalized = true;
        }
    }
    if (normalized && on_save_) {
        on_save_(base_config_);
    }

    suppress_changes_ = true;
    SetValues(base_config_);
    suppress_changes_ = false;
    CenterOnCursorMonitor();
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd_);
    visible_ = true;
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void SettingsWindow::Hide() {
    if (hwnd_ != nullptr && visible_) {
        NormalizeAllFields();
        ShowWindow(hwnd_, SW_HIDE);
    }
    visible_ = false;
}

void SettingsWindow::CreateControls() {
    const auto make_edit = [this](UINT id, int limit) {
        HWND edit = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0,
            hwnd_,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), instance_,
            nullptr);
        if (edit != nullptr) {
            SendMessageW(edit, EM_SETLIMITTEXT, static_cast<WPARAM>(limit), 0);
        }
        return edit;
    };

    const auto make_slider = [this](UINT id) {
        return CreateWindowExW(
            0, TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_AUTOTICKS | TBS_HORZ, 0,
            0, 0, 0, hwnd_,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), instance_,
            nullptr);
    };

    const auto make_button = [this](UINT id, const wchar_t* text, DWORD style) {
        return CreateWindowExW(
            0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style, 0,
            0, 0, 0, hwnd_,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), instance_,
            nullptr);
    };

    // Create in visual order so the tab order matches the layout.
    sliders_[0] = {make_slider(kIntervalSliderId),
                   &Config::reminder_interval_minutes, kIntervalValues, 5};
    sliders_[1] = {make_slider(kRestSliderId),
                   &Config::rest_duration_minutes, kRestValues, 3};
    sliders_[2] = {make_slider(kSnoozeSliderId),
                   &Config::snooze_duration_minutes, kSnoozeValues, 3};
    sliders_[3] = {make_slider(kMusicPlaySliderId),
                   &Config::music_play_seconds, kMusicPlayValues, 10};
    name_edit_ = make_edit(kNameEditId, 40);
    music_checkbox_ =
        make_button(kMusicCheckId, L"休息结束时播放音乐", BS_AUTOCHECKBOX);
    reset_button_ = make_button(kResetButtonId, L"恢复默认值", BS_PUSHBUTTON);
    close_button_ = make_button(kCloseButtonId, L"关闭", BS_DEFPUSHBUTTON);

    for (const Slider& slider : sliders_) {
        if (slider.track == nullptr) {
            continue;
        }
        SendMessageW(slider.track, TBM_SETRANGE, TRUE,
                     MAKELPARAM(0, slider.count - 1));
        SendMessageW(slider.track, TBM_SETTICFREQ, 1, 0);
        SendMessageW(slider.track, TBM_SETLINESIZE, 0, 1);
        SendMessageW(slider.track, TBM_SETPAGESIZE, 0, 1);
    }

    // The panel uses a fixed light palette; classic (unthemed) controls keep
    // that palette even when Windows runs in dark mode.
    HWND controls[] = {sliders_[0].track, sliders_[1].track,
                       sliders_[2].track, sliders_[3].track,
                       name_edit_,        music_checkbox_,
                       reset_button_,     close_button_};
    for (HWND control : controls) {
        if (control != nullptr) {
            SetWindowTheme(control, L"", L"");
        }
    }
}

void SettingsWindow::UpdateFonts() {
    HFONT new_font = CreateFontW(
        -MulDiv(10, static_cast<int>(dpi_), 72), 0, 0, 0, FW_NORMAL, FALSE,
        FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, kUiFontFamily);
    if (new_font == nullptr) {
        return;
    }

    if (font_ != nullptr) {
        DeleteObject(font_);
    }
    font_ = new_font;

    HWND controls[] = {sliders_[0].track, sliders_[1].track,
                       sliders_[2].track, sliders_[3].track,
                       name_edit_,        music_checkbox_,
                       reset_button_,     close_button_};
    for (HWND control : controls) {
        if (control != nullptr) {
            SendMessageW(control, WM_SETFONT,
                         reinterpret_cast<WPARAM>(font_), TRUE);
        }
    }
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void SettingsWindow::Layout() {
    const int side = static_cast<int>(dpi_);
    const auto scale = [side](int value) {
        return MulDiv(value, side, 96);
    };

    const int slider_height = scale(kTextHeight);
    for (int index = 0; index < 4; ++index) {
        if (sliders_[index].track == nullptr) {
            continue;
        }
        const int row_top = scale(kFirstRowY) + index * scale(kRowStep);
        MoveWindow(sliders_[index].track, scale(kControlX), row_top,
                   scale(kSliderWidth), slider_height, TRUE);
    }

    if (name_edit_ != nullptr) {
        const int edit_height = scale(kEditHeight);
        const int row_top = scale(kFirstRowY) + 4 * scale(kRowStep);
        MoveWindow(name_edit_, scale(kControlX),
                   row_top + (slider_height - edit_height) / 2,
                   scale(kClientWidth - kControlX - 20), edit_height, TRUE);
    }

    if (music_checkbox_ != nullptr) {
        const int edit_height = scale(kEditHeight);
        MoveWindow(music_checkbox_, scale(kLabelX), scale(kMusicRowY),
                   scale(kClientWidth - kLabelX - 20), edit_height, TRUE);
    }

    const int button_width = scale(92);
    const int button_height = scale(32);
    const int button_top = scale(kButtonTop);
    const int margin = scale(20);
    if (reset_button_ != nullptr) {
        MoveWindow(reset_button_, margin, button_top, scale(120),
                   button_height, TRUE);
    }
    if (close_button_ != nullptr) {
        MoveWindow(close_button_, scale(kClientWidth) - margin - button_width,
                   button_top, button_width, button_height, TRUE);
    }

    InvalidateRect(hwnd_, nullptr, TRUE);
}

void SettingsWindow::SetValues(const Config& config) {
    for (const Slider& slider : sliders_) {
        if (slider.track == nullptr) {
            continue;
        }
        const int index = NearestIndex(slider.values, slider.count,
                                       config.*(slider.member));
        SendMessageW(slider.track, TBM_SETPOS, TRUE, index);
    }
    if (name_edit_ != nullptr) {
        SetWindowTextW(name_edit_, config.user_name.c_str());
    }
    if (music_checkbox_ != nullptr) {
        SendMessageW(music_checkbox_, BM_SETCHECK,
                     config.music_enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::RestoreDefaults() {
    // Reset is immediate and keeps the panel open. Autostart is owned by the
    // tray menu and idle_threshold_seconds is config-file only, so neither is
    // touched here. The chosen music file is kept as well; resetting it would
    // silently delete a user-selected file.
    Config config = base_config_;
    config.reminder_interval_minutes = kDefaultReminderIntervalMinutes;
    config.rest_duration_minutes = kDefaultRestDurationMinutes;
    config.snooze_duration_minutes = kDefaultSnoozeDurationMinutes;
    config.music_play_seconds = kDefaultMusicPlaySeconds;
    config.user_name = kDefaultUserName;
    config = Sanitize(config);

    suppress_changes_ = true;
    SetValues(config);
    suppress_changes_ = false;

    base_config_ = config;
    if (on_save_) {
        on_save_(config);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
    if (sliders_[0].track != nullptr) {
        SetFocus(sliders_[0].track);
    }
}

void SettingsWindow::ApplyLive() {
    if (suppress_changes_ || !visible_) {
        return;
    }

    Config config = base_config_;
    for (const Slider& slider : sliders_) {
        if (slider.track == nullptr) {
            continue;
        }
        int index = static_cast<int>(
            SendMessageW(slider.track, TBM_GETPOS, 0, 0));
        if (index < 0) {
            index = 0;
        } else if (index >= slider.count) {
            index = slider.count - 1;
        }
        config.*(slider.member) = slider.values[index];
    }

    wchar_t buffer[64] = {};
    if (name_edit_ != nullptr) {
        GetWindowTextW(name_edit_, buffer, _countof(buffer));
    }
    config.user_name = buffer;
    if (music_checkbox_ != nullptr) {
        config.music_enabled =
            SendMessageW(music_checkbox_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    }
    config = Sanitize(config);

    // The panel does not expose these fields; keep the values it was opened
    // with so a tray toggle or a config-file edit is not overwritten.
    config.idle_threshold_seconds = base_config_.idle_threshold_seconds;
    config.autostart = base_config_.autostart;
    config.music_file = base_config_.music_file;

    if (!SameConfig(config, base_config_)) {
        base_config_ = config;
        if (on_save_) {
            on_save_(config);
        }
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::NormalizeName() {
    if (suppress_changes_ || !visible_ || name_edit_ == nullptr) {
        return;
    }
    wchar_t buffer[64] = {};
    GetWindowTextW(name_edit_, buffer, _countof(buffer));
    Config config = base_config_;
    config.user_name = buffer;
    const std::wstring normalized = Sanitize(config).user_name;

    suppress_changes_ = true;
    SetWindowTextW(name_edit_, normalized.c_str());
    suppress_changes_ = false;
    ApplyLive();
}

void SettingsWindow::NormalizeAllFields() {
    NormalizeName();
}

void SettingsWindow::CenterOnCursorMonitor() {
    POINT cursor = {};
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);

    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &info)) {
        return;
    }

    RECT window = {};
    GetWindowRect(hwnd_, &window);
    const int width = window.right - window.left;
    const int height = window.bottom - window.top;
    const int left = info.rcWork.left +
                     (info.rcWork.right - info.rcWork.left - width) / 2;
    const int top = info.rcWork.top +
                    (info.rcWork.bottom - info.rcWork.top - height) / 2;
    SetWindowPos(hwnd_, nullptr, left, top, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void SettingsWindow::PaintLabels(HDC dc) {
    static const wchar_t* const labels[] = {L"提醒间隔", L"休息时长",
                                            L"延后时长", L"音乐时长",
                                            L"显示名称"};

    if (font_ == nullptr) {
        return;
    }
    RECT client = {};
    GetClientRect(hwnd_, &client);
    const int scale = static_cast<int>(dpi_);

    // Repaint the whole client: slider values change length (for example
    // "30 分钟" -> "5 分钟"), and without clearing, the new text would be
    // drawn on top of the old digits.
    FillRect(dc, &client, background_);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(32, 32, 32));
    HGDIOBJ old = SelectObject(dc, font_);
    for (int index = 0; index < 5; ++index) {
        const int row_top =
            MulDiv(kFirstRowY, scale, 96) + index * MulDiv(kRowStep, scale, 96);
        const int text_height = MulDiv(kTextHeight, scale, 96);

        RECT label_rect = {MulDiv(kLabelX, scale, 96), row_top,
                           MulDiv(kControlX - 10, scale, 96),
                           row_top + text_height};
        DrawTextW(dc, labels[index], -1, &label_rect,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        if (index < 4) {
            const std::wstring value =
                std::to_wstring(base_config_.*(sliders_[index].member)) +
                (index == 3 ? L" 秒" : L" 分钟");
            RECT value_rect = {MulDiv(kValueX, scale, 96), row_top,
                               client.right - MulDiv(16, scale, 96),
                               row_top + text_height};
            DrawTextW(dc, value.c_str(), -1, &value_rect,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }
    SelectObject(dc, old);
}

LRESULT CALLBACK SettingsWindow::WndProc(HWND hwnd, UINT message,
                                         WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        auto* self = static_cast<SettingsWindow*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(self));
        return TRUE;
    }

    auto* self = reinterpret_cast<SettingsWindow*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    LRESULT result = self != nullptr
                         ? self->HandleMessage(message, wparam, lparam)
                         : DefWindowProcW(hwnd, message, wparam, lparam);

    if (message == WM_NCDESTROY) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        if (self != nullptr) {
            self->hwnd_ = nullptr;
        }
    }
    return result;
}

LRESULT SettingsWindow::HandleMessage(UINT message, WPARAM wparam,
                                      LPARAM lparam) {
    switch (message) {
        case WM_SIZE:
            Layout();
            return 0;

        case WM_DPICHANGED: {
            dpi_ = HIWORD(wparam);
            const RECT* suggested = reinterpret_cast<const RECT*>(lparam);
            RECT target = *suggested;
            SetWindowPos(hwnd_, nullptr, target.left, target.top,
                         target.right - target.left,
                         target.bottom - target.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            UpdateFonts();
            Layout();
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(hwnd_, &paint);
            PaintLabels(dc);
            EndPaint(hwnd_, &paint);
            return 0;
        }

        case WM_ERASEBKGND: {
            RECT client = {};
            GetClientRect(hwnd_, &client);
            FillRect(reinterpret_cast<HDC>(wparam), &client, background_);
            return 1;
        }

        case WM_CTLCOLORBTN: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            SetBkColor(dc, RGB(240, 240, 240));
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(background_);
        }

        case WM_HSCROLL:
            if (lparam != 0) {
                ApplyLive();
                return 0;
            }
            break;

        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case kResetButtonId:
                    RestoreDefaults();
                    return 0;
                case kCloseButtonId:
                case IDCANCEL:
                    Hide();
                    return 0;
                default:
                    break;
            }
            if (lparam != 0 && reinterpret_cast<HWND>(lparam) == name_edit_) {
                const WORD notification = HIWORD(wparam);
                if (notification == EN_CHANGE) {
                    ApplyLive();
                    return 0;
                }
                if (notification == EN_KILLFOCUS) {
                    NormalizeName();
                    return 0;
                }
            }
            if (lparam != 0 &&
                reinterpret_cast<HWND>(lparam) == music_checkbox_ &&
                HIWORD(wparam) == BN_CLICKED) {
                ApplyLive();
                return 0;
            }
            break;

        case WM_CLOSE:
            Hide();
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

}  // namespace rr

#include "reminder_window.h"

#include "win32/ui_theme.h"

#include <commctrl.h>
#include <shellscalingapi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <string>

namespace rr {
namespace {

constexpr wchar_t kWindowClass[] = L"RelaxationReminderWindow";
constexpr UINT kSnoozeButtonId = 2001;
constexpr UINT kCloseButtonId = 2002;
constexpr UINT kSettingsButtonId = 2003;

// Monochrome icon font used for the three action buttons. Segoe Fluent Icons
// ships with Windows 11; Segoe MDL2 Assets is the Windows 10 fallback and uses
// the same code points for these glyphs.
constexpr wchar_t kIconFontFamily[] = L"Segoe Fluent Icons";
constexpr wchar_t kIconFontFallback[] = L"Segoe MDL2 Assets";
constexpr wchar_t kSettingsGlyph[] = L"\xE713";  // Gear
constexpr wchar_t kSnoozeGlyph[] = L"\xE823";    // Clock with back arrow
constexpr wchar_t kCloseGlyph[] = L"\xE711";     // Cross

bool FontAvailable(const wchar_t* family) {
    HDC dc = GetDC(nullptr);
    LOGFONTW query = {};
    query.lfCharSet = DEFAULT_CHARSET;
    wcscpy_s(query.lfFaceName, family);
    bool found = false;
    EnumFontFamiliesExW(
        dc, &query,
        [](const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM param) -> int {
            *reinterpret_cast<bool*>(param) = true;
            return 0;
        },
        reinterpret_cast<LPARAM>(&found), 0);
    ReleaseDC(nullptr, dc);
    return found;
}

int ClampInt(int value, int low, int high) {
    return std::max(low, std::min(value, high));
}

std::wstring FormatClock(uint32_t seconds) {
    wchar_t buffer[16] = {};
    swprintf_s(buffer, L"%02u:%02u", seconds / 60, seconds % 60);
    return buffer;
}

void AddRoundedRect(Gdiplus::GraphicsPath* path, const Gdiplus::RectF& rect,
                    float radius) {
    const float diameter = radius * 2.0f;
    path->AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
    path->AddArc(rect.X + rect.Width - diameter, rect.Y, diameter, diameter,
                 270.0f, 90.0f);
    path->AddArc(rect.X + rect.Width - diameter,
                 rect.Y + rect.Height - diameter, diameter, diameter, 0.0f,
                 90.0f);
    path->AddArc(rect.X, rect.Y + rect.Height - diameter, diameter, diameter,
                 90.0f, 90.0f);
    path->CloseFigure();
}

void DrawPlaceholderCharacter(Gdiplus::Graphics& graphics,
                              const Gdiplus::RectF& area) {
    const float inset = area.Width * 0.08f;
    const Gdiplus::RectF face(area.X + inset, area.Y + inset,
                              area.Width - inset * 2.0f,
                              area.Height - inset * 2.0f);
    if (face.Width <= 0.0f || face.Height <= 0.0f) {
        return;
    }

    Gdiplus::GraphicsPath path;
    AddRoundedRect(&path, face, face.Width * 0.22f);

    Gdiplus::SolidBrush fill(Gdiplus::Color(255, 255, 183, 77));
    Gdiplus::Pen outline(Gdiplus::Color(255, 230, 126, 34),
                         std::max(1.5f, face.Width * 0.012f));
    graphics.FillPath(&fill, &path);
    graphics.DrawPath(&outline, &path);

    const Gdiplus::Color ink(255, 93, 64, 55);
    Gdiplus::SolidBrush ink_brush(ink);
    const float eye_radius = face.Width * 0.055f;
    const float eye_y = face.Y + face.Height * 0.38f;
    graphics.FillEllipse(&ink_brush, face.X + face.Width * 0.30f - eye_radius,
                         eye_y - eye_radius, eye_radius * 2.0f,
                         eye_radius * 2.0f);
    graphics.FillEllipse(&ink_brush, face.X + face.Width * 0.70f - eye_radius,
                         eye_y - eye_radius, eye_radius * 2.0f,
                         eye_radius * 2.0f);

    const Gdiplus::RectF smile(face.X + face.Width * 0.28f,
                               face.Y + face.Height * 0.46f,
                               face.Width * 0.44f, face.Height * 0.30f);
    Gdiplus::Pen smile_pen(ink, std::max(2.0f, face.Width * 0.032f));
    smile_pen.SetStartCap(Gdiplus::LineCapRound);
    smile_pen.SetEndCap(Gdiplus::LineCapRound);
    graphics.DrawArc(&smile_pen, smile, 25.0f, 130.0f);

    Gdiplus::SolidBrush cheek(Gdiplus::Color(110, 255, 138, 128));
    const float cheek_radius = face.Width * 0.055f;
    graphics.FillEllipse(&cheek, face.X + face.Width * 0.21f - cheek_radius,
                         face.Y + face.Height * 0.58f - cheek_radius,
                         cheek_radius * 2.0f, cheek_radius * 2.0f);
    graphics.FillEllipse(&cheek, face.X + face.Width * 0.79f - cheek_radius,
                         face.Y + face.Height * 0.58f - cheek_radius,
                         cheek_radius * 2.0f, cheek_radius * 2.0f);
}

}  // namespace

ReminderWindow::~ReminderWindow() {
    Destroy();
}

bool ReminderWindow::Create(HINSTANCE instance, HWND owner) {
    instance_ = instance;
    owner_ = owner;

    INITCOMMONCONTROLSEX common_controls = {};
    common_controls.dwSize = sizeof(common_controls);
    common_controls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&common_controls);

    icon_font_family_ =
        FontAvailable(kIconFontFamily) ? kIconFontFamily : kIconFontFallback;

    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    // Redraw the whole client area on every resize so the previous frame's
    // border, text and image cannot linger at the old size.
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = WndProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    window_class.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&window_class) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kWindowClass,
                            L"休息提醒", WS_POPUP | WS_CLIPCHILDREN, 0, 0,
                            320, 320, owner, nullptr, instance, this);
    if (hwnd_ == nullptr) {
        return false;
    }

    snooze_button_ = CreateWindowExW(
        0, L"BUTTON", kSnoozeGlyph,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 0, 0, hwnd_,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSnoozeButtonId)),
        instance, nullptr);
    close_button_ = CreateWindowExW(
        0, L"BUTTON", kCloseGlyph,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 0, 0, hwnd_,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kCloseButtonId)), instance,
        nullptr);
    settings_button_ = CreateWindowExW(
        0, L"BUTTON", kSettingsGlyph,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 0, 0, hwnd_,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsButtonId)),
        instance, nullptr);

    if (snooze_button_ == nullptr || close_button_ == nullptr ||
        settings_button_ == nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    // Icon-only buttons keep their meaning discoverable through tooltips.
    tooltip_ = CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, hwnd_, nullptr, instance, nullptr);
    if (tooltip_ != nullptr) {
        struct TooltipEntry {
            HWND button;
            const wchar_t* text;
        };
        const TooltipEntry entries[] = {
            {settings_button_, L"设置"},
            {snooze_button_, L"延后"},
            {close_button_, L"关闭"},
        };
        for (const TooltipEntry& entry : entries) {
            TOOLINFOW tool = {};
            tool.cbSize = sizeof(tool);
            tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
            tool.hwnd = hwnd_;
            tool.uId = reinterpret_cast<UINT_PTR>(entry.button);
            tool.lpszText = const_cast<LPWSTR>(entry.text);
            SendMessageW(tooltip_, TTM_ADDTOOLW, 0,
                         reinterpret_cast<LPARAM>(&tool));
        }
    }

    // Temporary layout size for the hidden window; ShowAtCursor resizes it to
    // one third of the monitor's short edge before the first display.
    UpdateFonts(320, 320);
    Layout(320, 320);
    return true;
}

void ReminderWindow::Destroy() {
    if (hwnd_ != nullptr && IsWindow(hwnd_) != FALSE) {
        DestroyWindow(hwnd_);
    }
    hwnd_ = nullptr;
    snooze_button_ = nullptr;
    close_button_ = nullptr;
    settings_button_ = nullptr;
    tooltip_ = nullptr;
    visible_ = false;
    current_side_ = 0;

    if (title_font_ != nullptr) {
        DeleteObject(title_font_);
        title_font_ = nullptr;
    }
    if (body_font_ != nullptr) {
        DeleteObject(body_font_);
        body_font_ = nullptr;
    }
    if (icon_font_ != nullptr) {
        DeleteObject(icon_font_);
        icon_font_ = nullptr;
    }
    title_font_height_ = 0;
    body_font_height_ = 0;
    icon_font_height_ = 0;

    ReleaseBuffer();
    character_.Clear();
}

void ReminderWindow::ShowAtCursor(uint64_t now_ms, bool snooze_used) {
    (void)now_ms;
    if (hwnd_ == nullptr) {
        return;
    }

    POINT cursor = {};
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);

    MONITORINFO monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &monitor_info)) {
        monitor_info.rcWork.left = 0;
        monitor_info.rcWork.top = 0;
        monitor_info.rcWork.right = GetSystemMetrics(SM_CXSCREEN);
        monitor_info.rcWork.bottom = GetSystemMetrics(SM_CYSCREEN);
        monitor = nullptr;
    }
    work_area_ = monitor_info.rcWork;

    UINT dpi_x = 96;
    UINT dpi_y = 96;
    if (monitor != nullptr) {
        if (FAILED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x,
                                    &dpi_y))) {
            dpi_x = 96;
        }
    }
    dpi_ = dpi_x == 0 ? 96 : dpi_x;

    const int work_width = work_area_.right - work_area_.left;
    const int work_height = work_area_.bottom - work_area_.top;

    // Start at one third of the monitor's short edge so the window scales
    // with the display instead of using a fixed logical size. GetMonitorInfo
    // returns physical pixels for this per-monitor-DPI-aware process, so no
    // extra DPI scaling is needed here.
    int short_edge = 0;
    if (monitor != nullptr) {
        short_edge =
            std::min(monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                     monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top);
    }
    if (short_edge <= 0) {
        short_edge = std::min(work_width, work_height);
    }
    int initial_side = std::max(1, short_edge / 3);
    initial_side =
        ClampInt(initial_side, 1, std::min(work_width, work_height));

    growth_.Reset(initial_side, work_width, work_height, 60.0);

    int left = cursor.x - initial_side / 2;
    int top = cursor.y - initial_side / 2;
    left = ClampInt(left, work_area_.left, work_area_.right - initial_side);
    top = ClampInt(top, work_area_.top, work_area_.bottom - initial_side);

    center_.x = left + initial_side / 2;
    center_.y = top + initial_side / 2;
    current_side_ = initial_side;
    current_left_ = left;
    current_top_ = top;

    SetWindowPos(hwnd_, HWND_TOPMOST, left, top, initial_side, initial_side,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);

    visible_ = true;
    status_text_.clear();
    title_text_.clear();
    character_.ResetAnimation();
    snooze_enabled_ = !snooze_used;
    if (snooze_button_ != nullptr) {
        EnableWindow(snooze_button_, snooze_enabled_ ? TRUE : FALSE);
    }

    UpdateFonts(initial_side, initial_side);
    Layout(initial_side, initial_side);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ReminderWindow::Hide() {
    if (hwnd_ != nullptr) {
        ShowWindow(hwnd_, SW_HIDE);
    }
    visible_ = false;
}

void ReminderWindow::Update(uint64_t now_ms, const StateMachine& machine) {
    if (!visible_ || hwnd_ == nullptr) {
        return;
    }
    if (!machine.IsVisible()) {
        Hide();
        return;
    }

    const int work_width = work_area_.right - work_area_.left;
    const int work_height = work_area_.bottom - work_area_.top;
    int side = static_cast<int>(
        std::lround(growth_.SideAt(machine.expansion_elapsed_ms())));
    side = ClampInt(side, 1, std::min(work_width, work_height));

    if (side != current_side_) {
        ResizeTo(side);
        // A size change only exposes the new border area, so the previous
        // border and background would remain inside the window. Repaint the
        // whole client area; the off-screen buffer keeps this flicker-free.
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    if (character_.Tick(now_ms)) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    const std::wstring status = BuildStatus(machine);
    if (status != status_text_) {
        status_text_ = status;
        RECT client = {};
        GetClientRect(hwnd_, &client);
        RECT line = StatusRect(client.right - client.left,
                               client.bottom - client.top);
        const int pad = std::max(2, static_cast<int>(dpi_ / 96));
        InflateRect(&line, pad, pad);
        InvalidateRect(hwnd_, &line, FALSE);
    }

    const std::wstring title = BuildTitle(machine);
    if (title != title_text_) {
        title_text_ = title;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    SetSnoozeEnabled(machine.CanSnooze());
}

void ReminderWindow::SetUserName(const std::wstring& name) {
    if (name == user_name_) {
        return;
    }
    user_name_ = name;
    Invalidate();
}

void ReminderWindow::SetSnoozeEnabled(bool enabled) {
    if (snooze_button_ == nullptr || enabled == snooze_enabled_) {
        return;
    }
    snooze_enabled_ = enabled;
    EnableWindow(snooze_button_, enabled ? TRUE : FALSE);
}

bool ReminderWindow::LoadCharacter(const std::wstring& path,
                                   std::wstring* error) {
    if (!character_.Load(path, error)) {
        return false;
    }
    Invalidate();
    return true;
}

void ReminderWindow::ClearCharacter() {
    character_.Clear();
    Invalidate();
}

void ReminderWindow::Invalidate() {
    if (hwnd_ != nullptr) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void ReminderWindow::ResizeTo(int side) {
    int left = center_.x - side / 2;
    int top = center_.y - side / 2;
    left = ClampInt(left, work_area_.left, work_area_.right - side);
    top = ClampInt(top, work_area_.top, work_area_.bottom - side);

    SetWindowPos(hwnd_, nullptr, left, top, side, side,
                 SWP_NOACTIVATE | SWP_NOZORDER);
    current_side_ = side;
    current_left_ = left;
    current_top_ = top;
}

void ReminderWindow::NotifyOwner(UINT message) {
    if (owner_ != nullptr) {
        PostMessageW(owner_, message, 0, 0);
    }
}

std::wstring ReminderWindow::BuildStatus(const StateMachine& machine) const {
    if (machine.timing_paused()) {
        return L"计时已暂停";
    }
    switch (machine.state()) {
        case State::ShowActive:
            return L"离开键鼠 " +
                   std::to_wstring(machine.idle_threshold_seconds()) +
                   L" 秒即开始休息";
        case State::RestRunning:
            return L"休息中 " + FormatClock(machine.rest_remaining_seconds()) +
                   L" / " + FormatClock(machine.rest_total_seconds());
        case State::RestPaused:
            return L"休息已暂停，离开键鼠 " +
                   std::to_wstring(machine.idle_threshold_seconds()) +
                   L" 秒后继续";
        case State::MusicPlaying:
            return L"音乐播放中 " +
                   FormatClock(machine.music_remaining_seconds()) + L" / " +
                   FormatClock(machine.music_total_seconds());
        default:
            return {};
    }
}

std::wstring ReminderWindow::BuildTitle(const StateMachine& machine) const {
    if (machine.state() == State::MusicPlaying) {
        return L"生产队的驴都不敢这么歇！";
    }
    return user_name_.empty() ? L"休息一下" : user_name_ + L"，休息一下";
}

void ReminderWindow::UpdateFonts(int width, int height) {
    const int side = std::min(width, height);
    const double scale = static_cast<double>(dpi_) / 96.0;
    const int title_height = ClampInt(
        static_cast<int>(std::lround(side * 0.075)),
        static_cast<int>(std::lround(16.0 * scale)),
        static_cast<int>(std::lround(34.0 * scale)));
    const int body_height = ClampInt(
        static_cast<int>(std::lround(side * 0.045)),
        static_cast<int>(std::lround(12.0 * scale)),
        static_cast<int>(std::lround(22.0 * scale)));
    // Icon glyphs need a little more room than body text to stay legible in
    // the action buttons.
    const int icon_height = ClampInt(
        static_cast<int>(std::lround(side * 0.062)),
        static_cast<int>(std::lround(14.0 * scale)),
        static_cast<int>(std::lround(30.0 * scale)));

    if (title_height == title_font_height_ &&
        body_height == body_font_height_ &&
        icon_height == icon_font_height_) {
        return;
    }

    HFONT new_title = CreateFontW(
        -title_height, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, kUiFontFamily);
    HFONT new_body = CreateFontW(
        -body_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, kUiFontFamily);
    HFONT new_icon = CreateFontW(
        -icon_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, icon_font_family_.c_str());

    HFONT old_title = title_font_;
    HFONT old_body = body_font_;
    HFONT old_icon = icon_font_;
    if (new_title != nullptr) {
        title_font_ = new_title;
    }
    if (new_body != nullptr) {
        body_font_ = new_body;
    }
    if (new_icon != nullptr) {
        icon_font_ = new_icon;
    }
    title_font_height_ = title_height;
    body_font_height_ = body_height;
    icon_font_height_ = icon_height;

    if (snooze_button_ != nullptr) {
        SendMessageW(snooze_button_, WM_SETFONT,
                     reinterpret_cast<WPARAM>(icon_font_), TRUE);
    }
    if (close_button_ != nullptr) {
        SendMessageW(close_button_, WM_SETFONT,
                     reinterpret_cast<WPARAM>(icon_font_), TRUE);
    }
    if (settings_button_ != nullptr) {
        SendMessageW(settings_button_, WM_SETFONT,
                     reinterpret_cast<WPARAM>(icon_font_), TRUE);
    }

    if (old_title != nullptr && old_title != title_font_) {
        DeleteObject(old_title);
    }
    if (old_body != nullptr && old_body != body_font_) {
        DeleteObject(old_body);
    }
    if (old_icon != nullptr && old_icon != icon_font_) {
        DeleteObject(old_icon);
    }
}

void ReminderWindow::Layout(int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    UpdateFonts(width, height);

    const int side = std::min(width, height);
    const double scale = static_cast<double>(dpi_) / 96.0;
    const int button_height = ClampInt(
        static_cast<int>(std::lround(side * 0.105)),
        static_cast<int>(std::lround(30.0 * scale)),
        static_cast<int>(std::lround(44.0 * scale)));
    const int gap = static_cast<int>(std::lround(8.0 * scale));
    const int margin = static_cast<int>(std::lround(10.0 * scale));

    // Three equal-width icon buttons, centered as a single row.
    int button_width = ClampInt(
        static_cast<int>(std::lround(side * 0.26)),
        static_cast<int>(std::lround(72.0 * scale)),
        static_cast<int>(std::lround(140.0 * scale)));
    const int max_width = (width - margin * 2 - gap * 2) / 3;
    if (max_width > 0) {
        button_width = std::min(button_width, max_width);
    }
    const int total_width = button_width * 3 + gap * 2;
    const int left = std::max(margin, (width - total_width) / 2);

    const int top =
        height - static_cast<int>(std::lround(side * 0.06)) - button_height;
    if (settings_button_ != nullptr) {
        MoveWindow(settings_button_, left, top, button_width, button_height,
                   TRUE);
    }
    if (snooze_button_ != nullptr) {
        MoveWindow(snooze_button_, left + button_width + gap, top, button_width,
                   button_height, TRUE);
    }
    if (close_button_ != nullptr) {
        MoveWindow(close_button_, left + (button_width + gap) * 2, top,
                   button_width, button_height, TRUE);
    }
}

void ReminderWindow::Paint(HDC dc) {
    RECT client = {};
    GetClientRect(hwnd_, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) {
        return;
    }

    if (!EnsureBuffer(dc, width, height)) {
        PaintContent(dc, width, height);
        return;
    }

    // Draw into the off-screen buffer first, then blit once so a partial
    // invalidation (the status line) never flashes the background.
    PaintContent(buffer_dc_, width, height);
    BitBlt(dc, 0, 0, width, height, buffer_dc_, 0, 0, SRCCOPY);
}

bool ReminderWindow::EnsureBuffer(HDC reference, int width, int height) {
    // Round the backing store up so the exponential growth (a resize every
    // tick) does not reallocate a bitmap on every frame.
    const int capacity_width = (width + 63) & ~63;
    const int capacity_height = (height + 63) & ~63;
    if (buffer_dc_ != nullptr && buffer_width_ >= width &&
        buffer_height_ >= height) {
        return true;
    }

    ReleaseBuffer();
    buffer_dc_ = CreateCompatibleDC(reference);
    if (buffer_dc_ == nullptr) {
        return false;
    }
    buffer_bitmap_ =
        CreateCompatibleBitmap(reference, capacity_width, capacity_height);
    if (buffer_bitmap_ == nullptr) {
        DeleteDC(buffer_dc_);
        buffer_dc_ = nullptr;
        return false;
    }
    buffer_previous_bitmap_ =
        static_cast<HBITMAP>(SelectObject(buffer_dc_, buffer_bitmap_));
    buffer_width_ = capacity_width;
    buffer_height_ = capacity_height;
    return true;
}

void ReminderWindow::ReleaseBuffer() {
    if (buffer_dc_ == nullptr) {
        return;
    }
    if (buffer_previous_bitmap_ != nullptr) {
        SelectObject(buffer_dc_, buffer_previous_bitmap_);
    }
    if (buffer_bitmap_ != nullptr) {
        DeleteObject(buffer_bitmap_);
    }
    DeleteDC(buffer_dc_);
    buffer_dc_ = nullptr;
    buffer_bitmap_ = nullptr;
    buffer_previous_bitmap_ = nullptr;
    buffer_width_ = 0;
    buffer_height_ = 0;
}

RECT ReminderWindow::StatusRect(int width, int height) const {
    const float side = static_cast<float>(std::min(width, height));
    const float image_side = side * 0.50f;
    const float image_top = side * 0.07f;
    const int image_bottom =
        static_cast<int>(std::lround(image_top + image_side));
    const int title_top =
        image_bottom + static_cast<int>(std::lround(side * 0.03f));
    const int title_height =
        static_cast<int>(std::lround(title_font_height_ * 1.35));
    const int status_top =
        title_top + title_height +
        static_cast<int>(std::lround(side * 0.005f));
    const int status_height =
        static_cast<int>(std::lround(body_font_height_ * 1.4));

    RECT rect = {0, status_top, width, status_top + status_height};
    return rect;
}

void ReminderWindow::PaintContent(HDC dc, int width, int height) {
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    Gdiplus::SolidBrush background(Gdiplus::Color(255, 255, 249, 235));
    graphics.FillRectangle(&background, 0, 0, width, height);

    const float border_width =
        std::max(1.0f, static_cast<float>(dpi_) / 96.0f);
    Gdiplus::Pen border(Gdiplus::Color(255, 214, 190, 160), border_width);
    graphics.DrawRectangle(&border, border_width / 2.0f, border_width / 2.0f,
                           static_cast<float>(width) - border_width,
                           static_cast<float>(height) - border_width);

    const float side = static_cast<float>(std::min(width, height));
    const float image_side = side * 0.50f;
    const float image_top = side * 0.07f;
    const Gdiplus::RectF image_rect(
        (static_cast<float>(width) - image_side) / 2.0f, image_top, image_side,
        image_side);
    if (character_.loaded()) {
        character_.Draw(graphics, image_rect);
    } else {
        DrawPlaceholderCharacter(graphics, image_rect);
    }
    graphics.Flush();

    const int image_bottom =
        static_cast<int>(std::lround(image_top + image_side));
    const int title_top =
        image_bottom + static_cast<int>(std::lround(side * 0.03f));
    const int title_height =
        static_cast<int>(std::lround(title_font_height_ * 1.35));
    const RECT status_rect = StatusRect(width, height);

    SetBkMode(dc, TRANSPARENT);
    if (title_font_ != nullptr) {
        std::wstring title = title_text_;
        if (title.empty()) {
            title = user_name_.empty() ? L"休息一下"
                                       : user_name_ + L"，休息一下";
        }
        HGDIOBJ old_font = SelectObject(dc, title_font_);
        SetTextColor(dc, RGB(62, 39, 35));
        RECT title_rect = {0, title_top, width, title_top + title_height};
        DrawTextW(dc, title.c_str(), -1, &title_rect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dc, old_font);
    }
    if (body_font_ != nullptr && !status_text_.empty()) {
        HGDIOBJ old_font = SelectObject(dc, body_font_);
        SetTextColor(dc, RGB(109, 76, 65));
        RECT line = status_rect;
        DrawTextW(dc, status_text_.c_str(), -1, &line,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dc, old_font);
    }
}

LRESULT CALLBACK ReminderWindow::WndProc(HWND hwnd, UINT message,
                                         WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        auto* self = static_cast<ReminderWindow*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(self));
        return TRUE;
    }

    auto* self = reinterpret_cast<ReminderWindow*>(
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

LRESULT ReminderWindow::HandleMessage(UINT message, WPARAM wparam,
                                      LPARAM lparam) {
    switch (message) {
        case WM_SIZE:
            Layout(LOWORD(lparam), HIWORD(lparam));
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(hwnd_, &paint);
            Paint(dc);
            EndPaint(hwnd_, &paint);
            return 0;
        }

        case WM_COMMAND:
            if (HIWORD(wparam) == BN_CLICKED) {
                switch (LOWORD(wparam)) {
                    case kSnoozeButtonId:
                        NotifyOwner(kSnoozeMessage);
                        return 0;
                    case kCloseButtonId:
                        NotifyOwner(kCloseMessage);
                        return 0;
                    case kSettingsButtonId:
                        NotifyOwner(kSettingsMessage);
                        return 0;
                    default:
                        break;
                }
            }
            break;

        case WM_CLOSE:
            NotifyOwner(kCloseMessage);
            return 0;

        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                NotifyOwner(kCloseMessage);
                return 0;
            }
            break;

        default:
            break;
    }
    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

}  // namespace rr

#include "splash_window.h"

#include "win32/ui_theme.h"

#include <shellscalingapi.h>

#include <string>

namespace rr {
namespace {

constexpr wchar_t kWindowClass[] = L"RelaxationReminderSplash";
constexpr UINT_PTR kHideTimerId = 1;

constexpr int kWidth = 340;
constexpr int kHeight = 108;

constexpr COLORREF kTitleColor = RGB(32, 32, 32);
constexpr COLORREF kFirstLineColor = RGB(70, 70, 70);
constexpr COLORREF kSecondLineColor = RGB(130, 130, 130);
constexpr COLORREF kBorderColor = RGB(204, 208, 214);

}  // namespace

SplashWindow::~SplashWindow() {
    Destroy();
}

bool SplashWindow::Create(HINSTANCE instance, HWND owner) {
    instance_ = instance;
    owner_ = owner;

    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_DROPSHADOW;
    window_class.lpfnWndProc = WndProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground =
        static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    window_class.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&window_class) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kWindowClass,
        L"", WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, kWidth, kHeight, owner,
        nullptr, instance, this);
    return hwnd_ != nullptr;
}

void SplashWindow::Destroy() {
    if (hwnd_ != nullptr && IsWindow(hwnd_) != FALSE) {
        DestroyWindow(hwnd_);
    }
    hwnd_ = nullptr;
    visible_ = false;

    if (title_font_ != nullptr) {
        DeleteObject(title_font_);
        title_font_ = nullptr;
    }
    if (body_font_ != nullptr) {
        DeleteObject(body_font_);
        body_font_ = nullptr;
    }
}

void SplashWindow::Show(const Config& config, UINT duration_ms) {
    if (hwnd_ == nullptr) {
        return;
    }

    const std::wstring name =
        config.user_name.empty() ? std::wstring(L"休息提醒") : config.user_name;
    title_ = name + L"，休息提醒已启动";
    line_first_ = L"将在 " +
                  std::to_wstring(config.reminder_interval_minutes) +
                  L" 分钟后提醒你休息。";
    line_second_ = L"可在托盘图标上右键打开设置。";

    PositionOnCursorMonitor();
    InvalidateRect(hwnd_, nullptr, TRUE);
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    KillTimer(hwnd_, kHideTimerId);
    SetTimer(hwnd_, kHideTimerId, duration_ms, nullptr);
    visible_ = true;
}

void SplashWindow::Hide() {
    if (hwnd_ != nullptr) {
        KillTimer(hwnd_, kHideTimerId);
        ShowWindow(hwnd_, SW_HIDE);
    }
    visible_ = false;
}

void SplashWindow::PositionOnCursorMonitor() {
    POINT cursor = {};
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);

    RECT work = {0, 0, GetSystemMetrics(SM_CXSCREEN),
                 GetSystemMetrics(SM_CYSCREEN)};
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (monitor != nullptr && GetMonitorInfoW(monitor, &info)) {
        work = info.rcWork;
    }

    UINT dpi_x = 96;
    UINT dpi_y = 96;
    if (monitor != nullptr) {
        if (FAILED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x,
                                    &dpi_y))) {
            dpi_x = 96;
        }
    }
    dpi_ = dpi_x == 0 ? 96 : dpi_x;
    RecreateFonts();

    const int width = MulDiv(kWidth, static_cast<int>(dpi_), 96);
    const int height = MulDiv(kHeight, static_cast<int>(dpi_), 96);
    const int left = work.left + (work.right - work.left - width) / 2;
    const int top = work.top + (work.bottom - work.top - height) * 2 / 5;

    SetWindowPos(hwnd_, HWND_TOPMOST, left, top, width, height,
                 SWP_NOACTIVATE);
}

void SplashWindow::RecreateFonts() {
    if (title_font_ != nullptr) {
        DeleteObject(title_font_);
        title_font_ = nullptr;
    }
    if (body_font_ != nullptr) {
        DeleteObject(body_font_);
        body_font_ = nullptr;
    }

    const int dpi = static_cast<int>(dpi_);
    title_font_ = CreateFontW(
        -MulDiv(13, dpi, 72), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, kUiFontFamily);
    body_font_ = CreateFontW(
        -MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, kUiFontFamily);
}

void SplashWindow::Paint(HDC dc) {
    RECT client = {};
    GetClientRect(hwnd_, &client);

    const int scale = static_cast<int>(dpi_);
    const auto px = [scale](int value) { return MulDiv(value, scale, 96); };

    FillRect(dc, &client,
             static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    HBRUSH border = CreateSolidBrush(kBorderColor);
    FrameRect(dc, &client, border);
    DeleteObject(border);

    SetBkMode(dc, TRANSPARENT);

    RECT title_rect = {px(16), px(14), client.right - px(16), px(40)};
    RECT first_rect = {px(16), px(44), client.right - px(16), px(64)};
    RECT second_rect = {px(16), px(66), client.right - px(16), px(86)};

    if (title_font_ != nullptr) {
        HGDIOBJ old = SelectObject(dc, title_font_);
        SetTextColor(dc, kTitleColor);
        DrawTextW(dc, title_.c_str(), -1, &title_rect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS |
                      DT_NOPREFIX);
        SelectObject(dc, old);
    }
    if (body_font_ != nullptr) {
        HGDIOBJ old = SelectObject(dc, body_font_);
        SetTextColor(dc, kFirstLineColor);
        DrawTextW(dc, line_first_.c_str(), -1, &first_rect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS |
                      DT_NOPREFIX);
        SetTextColor(dc, kSecondLineColor);
        DrawTextW(dc, line_second_.c_str(), -1, &second_rect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS |
                      DT_NOPREFIX);
        SelectObject(dc, old);
    }
}

LRESULT CALLBACK SplashWindow::WndProc(HWND hwnd, UINT message,
                                       WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        auto* self = static_cast<SplashWindow*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(self));
        return TRUE;
    }

    auto* self = reinterpret_cast<SplashWindow*>(
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

LRESULT SplashWindow::HandleMessage(UINT message, WPARAM wparam,
                                    LPARAM lparam) {
    switch (message) {
        case WM_TIMER:
            if (wparam == kHideTimerId) {
                Hide();
            }
            return 0;

        case WM_LBUTTONUP:
            Hide();
            return 0;

        case WM_CLOSE:
            Hide();
            return 0;

        case WM_DPICHANGED:
            dpi_ = HIWORD(wparam);
            RecreateFonts();
            InvalidateRect(hwnd_, nullptr, TRUE);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(hwnd_, &paint);
            Paint(dc);
            EndPaint(hwnd_, &paint);
            return 0;
        }

        default:
            break;
    }
    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

}  // namespace rr

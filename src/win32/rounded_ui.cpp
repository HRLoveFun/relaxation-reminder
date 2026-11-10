#include "win32/rounded_ui.h"

#include <dwmapi.h>

#include <commctrl.h>

#include <algorithm>
#include <cwchar>

#include "win32/gdiplus_include.h"

namespace rr {
namespace {

constexpr wchar_t kHoverProperty[] = L"RelaxationReminderButtonHover";
constexpr UINT_PTR kHoverSubclassId = 0x52525252;
constexpr wchar_t kIconFontFamily[] = L"Segoe Fluent Icons";
constexpr wchar_t kIconFontFallback[] = L"Segoe MDL2 Assets";

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

LRESULT CALLBACK HoverSubclassProc(HWND hwnd, UINT message, WPARAM wparam,
                                   LPARAM lparam, UINT_PTR,
                                   DWORD_PTR) {
    switch (message) {
        case WM_MOUSEMOVE:
            if (GetPropW(hwnd, kHoverProperty) == nullptr) {
                SetPropW(hwnd, kHoverProperty, reinterpret_cast<HANDLE>(1));
                TRACKMOUSEEVENT track = {};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                TrackMouseEvent(&track);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            break;
        case WM_MOUSELEAVE:
            if (GetPropW(hwnd, kHoverProperty) != nullptr) {
                RemovePropW(hwnd, kHoverProperty);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            break;
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, HoverSubclassProc, kHoverSubclassId);
            break;
        default:
            break;
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

// Defined locally so the project still compiles against Windows SDKs that
// predate the Windows 11 corner preference.
constexpr DWORD kDwmWindowCornerPreference = 33;
constexpr int kDwmCornerRound = 2;  // DWMWCP_ROUND
constexpr DWORD kDwmCaptionColor = 35;
constexpr DWORD kDwmTextColor = 36;
// The compositor rounds top-level windows with an 8 px radius at 96 DPI; the
// painted outline uses the same geometry so it follows the clipped edge.
constexpr float kWindowCornerRadius = 8.0f;
constexpr float kButtonCornerRadius = 6.0f;

Gdiplus::Color FromColorRef(COLORREF color, BYTE alpha = 255) {
    return Gdiplus::Color(alpha, GetRValue(color), GetGValue(color),
                          GetBValue(color));
}

void AddRoundRect(Gdiplus::GraphicsPath* path, const Gdiplus::RectF& rect,
                  float radius) {
    const float clamped =
        std::min(radius, std::min(rect.Width, rect.Height) / 2.0f);
    if (clamped <= 0.0f) {
        path->AddRectangle(rect);
        return;
    }
    const float diameter = clamped * 2.0f;
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

// Standard Windows push-button colours, so rounding the corners does not
// restyle the controls. Only the panel colour behind the corners differs.
constexpr ButtonColors kReminderButtons = {
    RGB(255, 249, 235), RGB(240, 240, 240),
    RGB(246, 246, 246), RGB(229, 229, 229), RGB(160, 160, 160),
    RGB(0, 0, 0),       RGB(240, 240, 240), RGB(204, 204, 204),
    RGB(109, 109, 109),
};

constexpr ButtonColors kSettingsButtons = {
    RGB(255, 255, 255), RGB(240, 240, 240),
    RGB(246, 246, 246), RGB(229, 229, 229), RGB(160, 160, 160),
    RGB(0, 0, 0),       RGB(240, 240, 240), RGB(204, 204, 204),
    RGB(109, 109, 109),
};

}  // namespace

const ButtonColors& ReminderButtonColors() { return kReminderButtons; }

const ButtonColors& SettingsButtonColors() { return kSettingsButtons; }

const wchar_t* IconFontFamily() {
    static const wchar_t* const family =
        FontAvailable(kIconFontFamily) ? kIconFontFamily : kIconFontFallback;
    return family;
}

void EnableButtonHover(HWND button) {
    if (button != nullptr) {
        SetWindowSubclass(button, HoverSubclassProc, kHoverSubclassId, 0);
    }
}

void ApplyRoundedWindowCorners(HWND hwnd, UINT dpi) {
    if (hwnd == nullptr) {
        return;
    }

    int preference = kDwmCornerRound;
    if (SUCCEEDED(DwmSetWindowAttribute(
            hwnd, kDwmWindowCornerPreference, &preference,
            sizeof(preference)))) {
        return;
    }

    // Pre-Windows 11 (or compositor disabled): clip the window to a rounded
    // region so the corners are still cut. The window owns the region.
    RECT client = {};
    GetClientRect(hwnd, &client);
    if (client.right <= 0 || client.bottom <= 0) {
        return;
    }
    const int radius = std::max(1, MulDiv(8, static_cast<int>(dpi), 96));
    HRGN region = CreateRoundRectRgn(0, 0, client.right + 1, client.bottom + 1,
                                     radius * 2, radius * 2);
    if (region != nullptr) {
        SetWindowRgn(hwnd, region, TRUE);
    }
}

void ApplyCaptionColors(HWND hwnd, COLORREF background, COLORREF text) {
    if (hwnd == nullptr) {
        return;
    }
    // DWM takes a COLORREF value packed into 32 bits for both attributes.
    const DWORD caption = static_cast<DWORD>(background);
    const DWORD caption_text = static_cast<DWORD>(text);
    DwmSetWindowAttribute(hwnd, kDwmCaptionColor, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd, kDwmTextColor, &caption_text,
                          sizeof(caption_text));
}

void DrawRoundedPopupBorder(HDC dc, int width, int height, COLORREF color,
                            UINT dpi) {
    if (dc == nullptr || width <= 0 || height <= 0) {
        return;
    }
    const float stroke = std::max(1.0f, static_cast<float>(dpi) / 96.0f);
    const float radius =
        std::max(0.0f, kWindowCornerRadius * static_cast<float>(dpi) / 96.0f -
                           stroke / 2.0f);

    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    Gdiplus::GraphicsPath path;
    AddRoundRect(&path,
                 Gdiplus::RectF(stroke / 2.0f, stroke / 2.0f,
                                static_cast<float>(width) - stroke,
                                static_cast<float>(height) - stroke),
                 radius);
    Gdiplus::Pen pen(FromColorRef(color), stroke);
    graphics.DrawPath(&pen, &path);
    graphics.Flush();
}

void DrawRoundedButton(const DRAWITEMSTRUCT& item, HFONT font,
                       const ButtonColors& colors, UINT dpi, bool prominent) {
    HDC dc = item.hDC;
    const RECT rect = item.rcItem;
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (dc == nullptr || width <= 0 || height <= 0) {
        return;
    }

    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool hot = (item.itemState & ODS_HOTLIGHT) != 0 ||
                     GetPropW(item.hwndItem, kHoverProperty) != nullptr;
    const bool focused = (item.itemState & ODS_FOCUS) != 0;

    // Erase the whole item with the panel colour first: the corners outside
    // the rounded shape would otherwise show the control's square default.
    HBRUSH background = CreateSolidBrush(colors.background);
    FillRect(dc, &rect, background);
    DeleteObject(background);

    const float scale = static_cast<float>(dpi) / 96.0f;
    const float border_width =
        prominent ? std::max(1.5f, 2.0f * scale) : std::max(1.0f, scale);
    const float radius = std::max(
        3.0f * scale,
        std::min(static_cast<float>(height) / 2.0f, kButtonCornerRadius * scale));

    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    const Gdiplus::RectF bounds(0.0f, 0.0f, static_cast<float>(width),
                                static_cast<float>(height));
    Gdiplus::GraphicsPath shape;
    AddRoundRect(&shape, bounds, radius);

    const COLORREF fill = disabled   ? colors.disabled_fill
                          : pressed  ? colors.fill_pressed
                          : hot      ? colors.fill_hot
                                     : colors.fill;
    Gdiplus::SolidBrush fill_brush(FromColorRef(fill));
    Gdiplus::Pen border_pen(
        FromColorRef(disabled ? colors.disabled_border : colors.border),
        border_width);
    graphics.FillPath(&fill_brush, &shape);
    graphics.DrawPath(&border_pen, &shape);

    if (focused && !disabled) {
        // A dotted ring inside the button mirrors the keyboard focus cue of a
        // standard push button without squaring off the rounded shape.
        const float inset = border_width + 2.0f;
        const Gdiplus::RectF focus_bounds(inset, inset,
                                          width - inset * 2.0f,
                                          height - inset * 2.0f);
        if (focus_bounds.Width > 1.0f && focus_bounds.Height > 1.0f) {
            Gdiplus::GraphicsPath focus;
            AddRoundRect(&focus, focus_bounds,
                         std::max(1.0f, radius - inset));
            Gdiplus::Pen focus_pen(FromColorRef(colors.text), 1.0f);
            focus_pen.SetDashStyle(Gdiplus::DashStyleDot);
            graphics.DrawPath(&focus_pen, &focus);
        }
    }
    graphics.Flush();

    wchar_t text[128] = {};
    GetWindowTextW(item.hwndItem, text, _countof(text));

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, disabled ? colors.disabled_text : colors.text);
    HGDIOBJ old_font = font != nullptr ? SelectObject(dc, font) : nullptr;
    RECT text_rect = rect;
    DrawTextW(dc, text, -1, &text_rect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS |
                  DT_NOPREFIX);
    if (old_font != nullptr) {
        SelectObject(dc, old_font);
    }
}

}  // namespace rr

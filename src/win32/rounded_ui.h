#pragma once

#include <windows.h>

namespace rr {

// Colours for the owner-drawn rounded push buttons. `background` is the
// parent panel colour painted behind the rounded corners, so a button can sit
// on a solid surface without showing its rectangular default background.
struct ButtonColors {
    COLORREF background;
    COLORREF fill;
    COLORREF fill_hot;
    COLORREF fill_pressed;
    COLORREF border;
    COLORREF text;
    COLORREF disabled_fill;
    COLORREF disabled_border;
    COLORREF disabled_text;
};

// Asks the Windows 11 compositor to round a top-level window's corners and
// falls back to an equivalent rounded window region on older systems. Call it
// again from WM_SIZE and WM_DPICHANGED so the fallback region tracks the
// window bounds.
void ApplyRoundedWindowCorners(HWND hwnd, UINT dpi);

// Sets the Windows 11 caption background and text colours so a captioned
// window does not show a title bar in a different colour from its client
// area. A no-op on systems without the colour attributes.
void ApplyCaptionColors(HWND hwnd, COLORREF background, COLORREF text);

// Draws the rounded outline of a borderless popup window. The radius matches
// the compositor's corner clip, so the stroke hugs the visible edge instead of
// being cut off at the corners.
void DrawRoundedPopupBorder(HDC dc, int width, int height, COLORREF color,
                            UINT dpi);

// Paints a rounded push button for a BS_OWNERDRAW BUTTON control. `prominent`
// gives the button the slightly heavier outline of a default action.
void DrawRoundedButton(const DRAWITEMSTRUCT& item, HFONT font,
                       const ButtonColors& colors, UINT dpi, bool prominent);

// Owner-drawn buttons do not highlight themselves on hover, so track the
// mouse and repaint when it enters or leaves. Call once per button.
void EnableButtonHover(HWND button);

const ButtonColors& ReminderButtonColors();
const ButtonColors& SettingsButtonColors();

// Icon font for button glyphs: Segoe Fluent Icons on Windows 11, falling back
// to Segoe MDL2 Assets (same code points for the glyphs used here).
const wchar_t* IconFontFamily();

}  // namespace rr

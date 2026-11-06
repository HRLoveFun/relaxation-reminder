#pragma once

#include <windows.h>

#include <string>

#include "core/config.h"

namespace rr {

// Small toast shown for a few seconds after the app starts so the user gets
// visible confirmation that the tray tool is running. It never takes focus
// and disappears on its own (or when clicked).
class SplashWindow {
public:
    static constexpr UINT kDefaultDurationMs = 4000;

    SplashWindow() = default;
    ~SplashWindow();

    SplashWindow(const SplashWindow&) = delete;
    SplashWindow& operator=(const SplashWindow&) = delete;

    bool Create(HINSTANCE instance, HWND owner);
    void Destroy();
    void Show(const Config& config, UINT duration_ms);
    void Hide();
    bool visible() const { return visible_; }
    HWND hwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam,
                                    LPARAM lparam);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);

    void PositionOnCursorMonitor();
    void RecreateFonts();
    void Paint(HDC dc);

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND owner_ = nullptr;
    HFONT title_font_ = nullptr;
    HFONT body_font_ = nullptr;
    UINT dpi_ = 96;
    std::wstring title_;
    std::wstring line_first_;
    std::wstring line_second_;
    bool visible_ = false;
};

}  // namespace rr

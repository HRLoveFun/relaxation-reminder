#pragma once

#include <windows.h>

#include <functional>
#include <string>

#include "core/config.h"

namespace rr {

// Small parameter panel opened from the reminder window or the tray menu.
// Edits are applied immediately (WYSIWYG); there is no save/cancel step.
// 恢复默认值 also applies right away. Autostart is controlled from the tray
// menu only, and idle_threshold_seconds is config-file only, so the panel
// never changes either of them.
class SettingsWindow {
public:
    using SaveHandler = std::function<void(const Config&)>;

    SettingsWindow() = default;
    ~SettingsWindow();

    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    bool Create(HINSTANCE instance, HWND owner);
    void Destroy();
    void Show(const Config& current, SaveHandler on_save);
    void Hide();
    bool visible() const { return visible_; }
    HWND hwnd() const { return hwnd_; }

private:
    struct Slider {
        HWND track = nullptr;
        uint32_t Config::*member = nullptr;
        const uint32_t* values = nullptr;
        int count = 0;
    };

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam,
                                    LPARAM lparam);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);

    void CreateControls();
    void UpdateFonts();
    void Layout();
    void SetValues(const Config& config);
    void RestoreDefaults();
    void ApplyLive();
    void NormalizeName();
    void NormalizeAllFields();
    void CenterOnCursorMonitor();
    void PaintLabels(HDC dc);

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND owner_ = nullptr;
    HWND name_edit_ = nullptr;
    HWND music_checkbox_ = nullptr;
    HWND reset_button_ = nullptr;
    HWND close_button_ = nullptr;
    Slider sliders_[4];

    HFONT font_ = nullptr;
    HBRUSH background_ = nullptr;
    UINT dpi_ = 96;
    Config base_config_;
    SaveHandler on_save_;
    bool visible_ = false;
    bool suppress_changes_ = false;
};

}  // namespace rr

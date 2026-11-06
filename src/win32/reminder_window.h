#pragma once

#include <windows.h>

#include <cstdint>
#include <string>

#include "core/growth.h"
#include "core/state_machine.h"
#include "win32/character_image.h"

namespace rr {

class ReminderWindow {
public:
    static constexpr UINT kSnoozeMessage = WM_APP + 20;
    static constexpr UINT kCloseMessage = WM_APP + 21;
    static constexpr UINT kSettingsMessage = WM_APP + 22;

    ReminderWindow() = default;
    ~ReminderWindow();

    ReminderWindow(const ReminderWindow&) = delete;
    ReminderWindow& operator=(const ReminderWindow&) = delete;

    bool Create(HINSTANCE instance, HWND owner);
    void Destroy();

    void ShowAtCursor(uint64_t now_ms, bool snooze_used);
    void Hide();
    void Update(uint64_t now_ms, const StateMachine& machine);
    void SetSnoozeEnabled(bool enabled);
    void SetUserName(const std::wstring& name);

    bool LoadCharacter(const std::wstring& path, std::wstring* error);
    void ClearCharacter();
    bool has_character() const { return character_.loaded(); }
    void Invalidate();

    bool visible() const { return visible_; }
    HWND hwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam,
                                    LPARAM lparam);

    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    void Paint(HDC dc);
    void Layout(int width, int height);
    void UpdateFonts(int width, int height);
    void ResizeTo(int side);
    void NotifyOwner(UINT message);
    std::wstring BuildStatus(const StateMachine& machine) const;

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND owner_ = nullptr;
    HWND snooze_button_ = nullptr;
    HWND close_button_ = nullptr;
    HWND settings_button_ = nullptr;

    HFONT title_font_ = nullptr;
    HFONT body_font_ = nullptr;
    int title_font_height_ = 0;
    int body_font_height_ = 0;

    bool visible_ = false;
    GrowthCurve growth_;
    RECT work_area_ = {};
    POINT center_ = {};
    UINT dpi_ = 96;
    int current_side_ = 0;
    int current_left_ = 0;
    int current_top_ = 0;
    std::wstring status_text_;
    std::wstring user_name_;
    bool snooze_enabled_ = true;

    CharacterImage character_;
};

}  // namespace rr

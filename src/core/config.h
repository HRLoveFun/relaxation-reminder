#pragma once

#include <cstdint>
#include <string>

namespace rr {

inline constexpr uint32_t kDefaultReminderIntervalMinutes = 30;
inline constexpr uint32_t kDefaultRestDurationMinutes = 5;
inline constexpr uint32_t kDefaultSnoozeDurationMinutes = 5;
inline constexpr uint32_t kDefaultIdleThresholdSeconds = 10;
inline constexpr bool kDefaultAutostart = true;

inline constexpr uint32_t kMaxMinutes = 1440;
inline constexpr uint32_t kMaxIdleSeconds = 3600;

inline constexpr wchar_t kDefaultUserName[] = L"芬宝";
// Used when the configured name is empty or contains illegal characters.
inline constexpr wchar_t kFallbackUserName[] = L"牛马";
inline constexpr size_t kMaxUserNameLength = 20;

struct Config {
    uint32_t reminder_interval_minutes = kDefaultReminderIntervalMinutes;
    uint32_t rest_duration_minutes = kDefaultRestDurationMinutes;
    uint32_t snooze_duration_minutes = kDefaultSnoozeDurationMinutes;
    uint32_t idle_threshold_seconds = kDefaultIdleThresholdSeconds;
    bool autostart = kDefaultAutostart;
    std::wstring user_name = kDefaultUserName;
    std::string character_image_file;
};

bool IsValidMinutes(uint32_t value);
bool IsValidIdleSeconds(uint32_t value);

// True when the trimmed input is non-empty and has no control characters.
bool IsUserNameInputValid(const std::wstring& value);

// Invalid values fall back to defaults; image names containing path
// separators are rejected so the file stays inside the app data directory.
Config Sanitize(const Config& value);

}  // namespace rr

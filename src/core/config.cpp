#include "config.h"

namespace rr {
namespace {

// Keeps the display name on one line, trims surrounding spaces and caps the
// length; empty or illegal input falls back to "牛马".
std::wstring SanitizeUserName(const std::wstring& value) {
    if (!IsUserNameInputValid(value)) {
        return kFallbackUserName;
    }

    const size_t first = value.find_first_not_of(L' ');
    const size_t last = value.find_last_not_of(L' ');
    std::wstring cleaned = value.substr(first, last - first + 1);

    if (cleaned.size() > kMaxUserNameLength) {
        cleaned.resize(kMaxUserNameLength);
        // Do not leave half of a surrogate pair behind.
        if (!cleaned.empty() && cleaned.back() >= 0xD800 &&
            cleaned.back() <= 0xDBFF) {
            cleaned.pop_back();
        }
        const size_t end = cleaned.find_last_not_of(L' ');
        cleaned.resize(end == std::wstring::npos ? 0 : end + 1);
    }

    return cleaned.empty() ? kFallbackUserName : cleaned;
}

}  // namespace

bool IsUserNameInputValid(const std::wstring& value) {
    bool has_visible = false;
    for (const wchar_t character : value) {
        if (character < 0x20 || character == 0x7F ||
            (character >= 0x80 && character <= 0x9F)) {
            return false;
        }
        if (character != L' ') {
            has_visible = true;
        }
    }
    return has_visible;
}

bool IsValidMinutes(uint32_t value) {
    return value >= 1 && value <= kMaxMinutes;
}

bool IsValidIdleSeconds(uint32_t value) {
    return value >= 1 && value <= kMaxIdleSeconds;
}

Config Sanitize(const Config& value) {
    Config result = value;

    if (!IsValidMinutes(result.reminder_interval_minutes)) {
        result.reminder_interval_minutes = kDefaultReminderIntervalMinutes;
    }
    if (!IsValidMinutes(result.rest_duration_minutes)) {
        result.rest_duration_minutes = kDefaultRestDurationMinutes;
    }
    if (!IsValidMinutes(result.snooze_duration_minutes)) {
        result.snooze_duration_minutes = kDefaultSnoozeDurationMinutes;
    }
    if (!IsValidIdleSeconds(result.idle_threshold_seconds)) {
        result.idle_threshold_seconds = kDefaultIdleThresholdSeconds;
    }
    result.user_name = SanitizeUserName(result.user_name);
    if (result.character_image_file.size() > 260 ||
        result.character_image_file.find_first_of("\\/") != std::string::npos) {
        result.character_image_file.clear();
    }

    return result;
}

}  // namespace rr

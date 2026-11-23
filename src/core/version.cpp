#include "core/version.h"

namespace rr {

bool ParseVersion(const std::string& text, VersionNumber* out) {
    if (out == nullptr) {
        return false;
    }

    VersionNumber value;
    size_t index = 0;
    while (index < text.size() &&
           (text[index] == ' ' || text[index] == '\t')) {
        ++index;
    }
    if (index < text.size() && (text[index] == 'v' || text[index] == 'V')) {
        ++index;
    }

    for (int part = 0; part < 4; ++part) {
        if (index >= text.size() || text[index] < '0' || text[index] > '9') {
            return false;
        }
        unsigned number = 0;
        while (index < text.size() && text[index] >= '0' &&
               text[index] <= '9') {
            number = number * 10 + static_cast<unsigned>(text[index] - '0');
            if (number > 999999) {
                return false;
            }
            ++index;
        }
        value.parts[part] = number;

        if (part < 3) {
            if (index >= text.size() || text[index] != '.') {
                return false;
            }
            ++index;
        }
    }

    *out = value;
    return true;
}

int CompareVersions(const VersionNumber& left, const VersionNumber& right) {
    for (int part = 0; part < 4; ++part) {
        if (left.parts[part] != right.parts[part]) {
            return left.parts[part] < right.parts[part] ? -1 : 1;
        }
    }
    return 0;
}

}  // namespace rr

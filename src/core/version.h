#pragma once

#include <string>

namespace rr {

struct VersionNumber {
    unsigned parts[4] = {0, 0, 0, 0};
};

// Accepts "0.11.25.6", "v0.11.25.6" and a trailing suffix such as
// "v0.11.25.6-beta". Returns false when fewer than four numeric parts exist.
bool ParseVersion(const std::string& text, VersionNumber* out);

// Returns <0, 0 or >0 like strcmp.
int CompareVersions(const VersionNumber& left, const VersionNumber& right);

}  // namespace rr

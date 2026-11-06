#pragma once

namespace rr {

// Single UI font family for every custom-drawn window and child control.
// Microsoft YaHei UI ships with Windows 11 and covers both Chinese and Latin
// text, so mixed-language labels no longer switch fonts mid-string.
inline constexpr wchar_t kUiFontFamily[] = L"Microsoft YaHei UI";

}  // namespace rr

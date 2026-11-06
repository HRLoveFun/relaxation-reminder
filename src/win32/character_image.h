#pragma once

#include "win32/gdiplus_include.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rr {

inline constexpr uint64_t kMaxCharacterFileBytes = 10ull * 1024ull * 1024ull;
inline constexpr int kMaxCharacterDimension = 1024;
inline constexpr UINT kMaxCharacterFrames = 500;

bool IsSupportedCharacterExtension(const std::wstring& path);

// Loads a user character image or animated GIF. The image file stays locked
// by GDI+ for the lifetime of this object.
class CharacterImage {
public:
    CharacterImage() = default;
    ~CharacterImage();

    CharacterImage(const CharacterImage&) = delete;
    CharacterImage& operator=(const CharacterImage&) = delete;

    bool Load(const std::wstring& path, std::wstring* error);
    void Clear();

    bool loaded() const { return image_ != nullptr; }
    UINT frame_count() const { return frame_count_; }
    void ResetAnimation();
    bool Tick(uint64_t now_ms);
    void Draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& area) const;

private:
    void Release();
    UINT DelayFor(UINT frame) const;

    Gdiplus::Image* image_ = nullptr;
    UINT frame_count_ = 1;
    UINT current_frame_ = 0;
    std::vector<UINT> delays_ms_;
    bool animated_ = false;
    bool animation_started_ = false;
    uint64_t next_frame_ms_ = 0;
};

}  // namespace rr

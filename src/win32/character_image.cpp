#include "character_image.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <utility>
#include <vector>

namespace rr {
namespace {

std::wstring LowercaseExtension(const std::wstring& path) {
    const size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) {
        return {};
    }
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos && dot < slash) {
        return {};
    }
    std::wstring extension = path.substr(dot + 1);
    for (wchar_t& character : extension) {
        character = static_cast<wchar_t>(std::towlower(character));
    }
    return extension;
}

bool ReadFrameDelays(Gdiplus::Image* image, UINT frame_count,
                     std::vector<UINT>* delays) {
    delays->assign(frame_count, 100);

    const UINT bytes = image->GetPropertyItemSize(PropertyTagFrameDelay);
    if (bytes < sizeof(Gdiplus::PropertyItem)) {
        return false;
    }

    std::vector<BYTE> buffer(bytes);
    auto* item = reinterpret_cast<Gdiplus::PropertyItem*>(buffer.data());
    if (image->GetPropertyItem(PropertyTagFrameDelay, bytes, item) !=
            Gdiplus::Ok ||
        item->value == nullptr) {
        return false;
    }

    const UINT value_count = item->length / sizeof(LONG);
    const auto* values = static_cast<const LONG*>(item->value);
    for (UINT frame = 0; frame < frame_count; ++frame) {
        const LONG hundredths = frame < value_count ? values[frame] : 10;
        long milliseconds = static_cast<long>(hundredths) * 10;
        milliseconds = std::max(20L, std::min(1000L, milliseconds));
        (*delays)[frame] = static_cast<UINT>(milliseconds);
    }
    return true;
}

}  // namespace

bool IsSupportedCharacterExtension(const std::wstring& path) {
    const std::wstring extension = LowercaseExtension(path);
    return extension == L"png" || extension == L"jpg" ||
           extension == L"jpeg" || extension == L"bmp" || extension == L"gif";
}

CharacterImage::~CharacterImage() {
    Release();
}

bool CharacterImage::Load(const std::wstring& path, std::wstring* error) {
    const auto fail = [error](const wchar_t* message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    WIN32_FILE_ATTRIBUTE_DATA attributes = {};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard,
                              &attributes)) {
        return fail(L"文件不存在或无法访问。");
    }
    if ((attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return fail(L"所选路径不是文件。");
    }

    ULARGE_INTEGER size = {};
    size.LowPart = attributes.nFileSizeLow;
    size.HighPart = attributes.nFileSizeHigh;
    if (size.QuadPart == 0) {
        return fail(L"文件为空。");
    }
    if (size.QuadPart > kMaxCharacterFileBytes) {
        return fail(L"文件超过 10 MB。");
    }
    if (!IsSupportedCharacterExtension(path)) {
        return fail(L"仅支持 PNG、JPEG、BMP、GIF。");
    }

    Gdiplus::Image* candidate = Gdiplus::Image::FromFile(path.c_str(), FALSE);
    if (candidate == nullptr) {
        return fail(L"图片解码失败。");
    }
    if (candidate->GetLastStatus() != Gdiplus::Ok) {
        delete candidate;
        return fail(L"图片解码失败。");
    }

    const UINT width = candidate->GetWidth();
    const UINT height = candidate->GetHeight();
    if (width == 0 || height == 0 ||
        width > static_cast<UINT>(kMaxCharacterDimension) ||
        height > static_cast<UINT>(kMaxCharacterDimension)) {
        delete candidate;
        return fail(L"单帧尺寸不能超过 1024×1024。");
    }

    const GUID* time_dimension = &Gdiplus::FrameDimensionTime;
    UINT frame_count = candidate->GetFrameCount(time_dimension);
    if (frame_count == 0) {
        frame_count = 1;
    }
    if (frame_count > kMaxCharacterFrames) {
        delete candidate;
        return fail(L"GIF 帧数不能超过 500。");
    }

    std::vector<UINT> delays;
    if (frame_count > 1) {
        candidate->SelectActiveFrame(time_dimension, 0);
        ReadFrameDelays(candidate, frame_count, &delays);
    }

    Release();
    image_ = candidate;
    frame_count_ = frame_count;
    current_frame_ = 0;
    delays_ms_ = std::move(delays);
    animated_ = frame_count > 1;
    animation_started_ = false;
    next_frame_ms_ = 0;
    return true;
}

void CharacterImage::Clear() {
    Release();
}

void CharacterImage::ResetAnimation() {
    if (!animated_) {
        return;
    }
    current_frame_ = 0;
    image_->SelectActiveFrame(&Gdiplus::FrameDimensionTime, 0);
    animation_started_ = false;
    next_frame_ms_ = 0;
}

bool CharacterImage::Tick(uint64_t now_ms) {
    if (image_ == nullptr || !animated_) {
        return false;
    }
    if (!animation_started_) {
        animation_started_ = true;
        next_frame_ms_ = now_ms + DelayFor(current_frame_);
        return false;
    }
    if (now_ms < next_frame_ms_) {
        return false;
    }

    current_frame_ = (current_frame_ + 1) % frame_count_;
    image_->SelectActiveFrame(&Gdiplus::FrameDimensionTime, current_frame_);
    next_frame_ms_ = now_ms + DelayFor(current_frame_);
    return true;
}

void CharacterImage::Draw(Gdiplus::Graphics& graphics,
                          const Gdiplus::RectF& area) const {
    if (image_ == nullptr || area.Width <= 0.0f || area.Height <= 0.0f) {
        return;
    }

    const double image_width = static_cast<double>(image_->GetWidth());
    const double image_height = static_cast<double>(image_->GetHeight());
    if (image_width <= 0.0 || image_height <= 0.0) {
        return;
    }

    const double scale =
        std::min(area.Width / image_width, area.Height / image_height);
    const double width = image_width * scale;
    const double height = image_height * scale;
    const Gdiplus::RectF destination(
        area.X + static_cast<Gdiplus::REAL>((area.Width - width) / 2.0),
        area.Y + static_cast<Gdiplus::REAL>((area.Height - height) / 2.0),
        static_cast<Gdiplus::REAL>(width), static_cast<Gdiplus::REAL>(height));

    graphics.SetInterpolationMode(
        Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    graphics.DrawImage(image_, destination);
}

void CharacterImage::Release() {
    delete image_;
    image_ = nullptr;
    frame_count_ = 1;
    current_frame_ = 0;
    delays_ms_.clear();
    animated_ = false;
    animation_started_ = false;
    next_frame_ms_ = 0;
}

UINT CharacterImage::DelayFor(UINT frame) const {
    if (delays_ms_.empty()) {
        return 100;
    }
    return delays_ms_[frame % delays_ms_.size()];
}

}  // namespace rr

#include "rest_sound.h"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "core/config.h"

namespace rr {
namespace {

// Resource id of the built-in music clip in app.rc (type WAVE).
constexpr int kMusicResourceId = 2;

// Small buffers keep the delay between "user touched the keyboard" and the
// first faded sample short. Four of them give the 50 ms UI timer plenty of
// headroom against underruns.
constexpr size_t kBufferCount = 4;
constexpr uint32_t kBufferMilliseconds = 40;
constexpr uint32_t kFadeMilliseconds = 600;

uint16_t ReadLe16(const unsigned char* bytes) {
    return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
}

uint32_t ReadLe32(const unsigned char* bytes) {
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

bool ChunkIdEquals(const unsigned char* id, const char* expected) {
    return id[0] == static_cast<unsigned char>(expected[0]) &&
           id[1] == static_cast<unsigned char>(expected[1]) &&
           id[2] == static_cast<unsigned char>(expected[2]) &&
           id[3] == static_cast<unsigned char>(expected[3]);
}

void SetError(std::wstring* error, const wchar_t* message) {
    if (error != nullptr) {
        *error = message;
    }
}

struct Clip {
    WAVEFORMATEX format = {};
    std::vector<BYTE> pcm;
    uint32_t block_align = 0;
    uint64_t frames = 0;
};

struct Buffer {
    WAVEHDR header = {};
    std::vector<BYTE> data;
};

bool ParseWav(const unsigned char* bytes, size_t size, Clip* clip) {
    if (size < 12 || !ChunkIdEquals(bytes, "RIFF") ||
        !ChunkIdEquals(bytes + 8, "WAVE")) {
        return false;
    }

    const unsigned char* fmt = nullptr;
    uint32_t fmt_size = 0;
    const unsigned char* data = nullptr;
    uint32_t data_size = 0;
    size_t offset = 12;
    for (int chunk = 0; chunk < 64 && offset + 8 <= size; ++chunk) {
        const unsigned char* header = bytes + offset;
        const uint64_t chunk_size = ReadLe32(header + 4);
        const uint64_t data_offset = offset + 8;
        if (chunk_size > size - data_offset) {
            return false;
        }
        if (ChunkIdEquals(header, "fmt ") && chunk_size >= 16 && fmt == nullptr) {
            fmt = bytes + data_offset;
            fmt_size = static_cast<uint32_t>(chunk_size);
        } else if (ChunkIdEquals(header, "data") && data == nullptr) {
            data = bytes + data_offset;
            data_size = static_cast<uint32_t>(chunk_size);
        }
        offset = static_cast<size_t>(data_offset + chunk_size +
                                     (chunk_size & 1u));
    }

    (void)fmt_size;
    if (fmt == nullptr || data == nullptr || data_size == 0) {
        return false;
    }

    const uint16_t audio_format = ReadLe16(fmt);
    const uint16_t channels = ReadLe16(fmt + 2);
    const uint32_t sample_rate = ReadLe32(fmt + 4);
    const uint16_t bits_per_sample = ReadLe16(fmt + 14);
    if (audio_format != 1 || channels < 1 || channels > 2 ||
        sample_rate < 8000 || sample_rate > 48000 ||
        (bits_per_sample != 8 && bits_per_sample != 16)) {
        return false;
    }

    const uint32_t block_align = channels * (bits_per_sample / 8u);
    if (block_align == 0) {
        return false;
    }
    const uint32_t usable = data_size - (data_size % block_align);
    if (usable == 0) {
        return false;
    }

    clip->format.wFormatTag = WAVE_FORMAT_PCM;
    clip->format.nChannels = channels;
    clip->format.nSamplesPerSec = sample_rate;
    clip->format.wBitsPerSample = bits_per_sample;
    clip->format.nBlockAlign = static_cast<WORD>(block_align);
    clip->format.nAvgBytesPerSec = sample_rate * block_align;
    clip->format.cbSize = 0;
    clip->block_align = block_align;
    clip->pcm.assign(data, data + usable);
    clip->frames = usable / block_align;
    return true;
}

bool ReadWholeFile(const std::wstring& path, std::vector<BYTE>* bytes) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 ||
        static_cast<uint64_t>(size.QuadPart) > kMaxMusicFileBytes) {
        CloseHandle(file);
        return false;
    }

    bytes->resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    const bool ok =
        ReadFile(file, bytes->data(), static_cast<DWORD>(bytes->size()), &read,
                 nullptr) != FALSE &&
        read == bytes->size();
    CloseHandle(file);
    if (!ok) {
        bytes->clear();
    }
    return ok;
}

bool LoadEmbeddedClip(Clip* clip) {
    HRSRC resource =
        FindResourceW(nullptr, MAKEINTRESOURCEW(kMusicResourceId), L"WAVE");
    if (resource == nullptr) {
        return false;
    }
    HGLOBAL loaded = LoadResource(nullptr, resource);
    if (loaded == nullptr) {
        return false;
    }
    const void* data = LockResource(loaded);
    const DWORD size = SizeofResource(nullptr, resource);
    if (data == nullptr || size == 0) {
        return false;
    }
    return ParseWav(static_cast<const unsigned char*>(data), size, clip);
}

HWAVEOUT g_device = nullptr;
Clip g_clip;
std::array<Buffer, kBufferCount> g_buffers;
bool g_active = false;
bool g_fading = false;
uint64_t g_samples_per_buffer = 0;
uint64_t g_next_sample = 0;
uint64_t g_clip_position = 0;
uint64_t g_fade_start_sample = 0;
uint64_t g_fade_samples = 0;
uint64_t g_fade_started_tick = 0;

double VolumeAt(uint64_t sample) {
    if (!g_fading) {
        return 1.0;
    }
    if (sample <= g_fade_start_sample) {
        return 1.0;
    }
    const uint64_t offset = sample - g_fade_start_sample;
    if (offset >= g_fade_samples) {
        return 0.0;
    }
    return 1.0 - static_cast<double>(offset) /
                     static_cast<double>(g_fade_samples);
}

void FillBuffer(Buffer* buffer) {
    const uint32_t channels = g_clip.format.nChannels;
    const bool sixteen_bit = g_clip.format.wBitsPerSample == 16;
    const uint64_t frames = g_samples_per_buffer;
    BYTE* destination = buffer->data.data();
    uint64_t position = g_clip_position;

    for (uint64_t frame = 0; frame < frames; ++frame) {
        const double volume = VolumeAt(g_next_sample + frame);
        const BYTE* source = g_clip.pcm.data() + position * g_clip.block_align;
        if (sixteen_bit) {
            auto* out = reinterpret_cast<int16_t*>(destination +
                                                   frame * g_clip.block_align);
            const auto* in = reinterpret_cast<const int16_t*>(source);
            for (uint32_t channel = 0; channel < channels; ++channel) {
                long scaled = std::lround(in[channel] * volume);
                scaled = std::max<long>(-32768, std::min<long>(32767, scaled));
                out[channel] = static_cast<int16_t>(scaled);
            }
        } else {
            BYTE* out = destination + frame * g_clip.block_align;
            for (uint32_t channel = 0; channel < channels; ++channel) {
                const int centered = static_cast<int>(source[channel]) - 128;
                long scaled = 128 + std::lround(centered * volume);
                scaled = std::max<long>(0, std::min<long>(255, scaled));
                out[channel] = static_cast<BYTE>(scaled);
            }
        }
        ++position;
        if (position >= g_clip.frames) {
            position = 0;
        }
    }

    g_clip_position = position;
    g_next_sample += frames;
    buffer->header.dwBufferLength =
        static_cast<DWORD>(frames * g_clip.block_align);
}

bool QueueBuffer(Buffer* buffer) {
    if (g_device == nullptr) {
        return false;
    }
    FillBuffer(buffer);
    return waveOutWrite(g_device, &buffer->header, sizeof(WAVEHDR)) ==
           MMSYSERR_NOERROR;
}

void ReleaseDevice() {
    if (g_device != nullptr) {
        waveOutReset(g_device);
        for (Buffer& buffer : g_buffers) {
            if ((buffer.header.dwFlags & WHDR_PREPARED) != 0) {
                waveOutUnprepareHeader(g_device, &buffer.header,
                                       sizeof(WAVEHDR));
            }
        }
        waveOutClose(g_device);
        g_device = nullptr;
    }
    for (Buffer& buffer : g_buffers) {
        buffer.header = {};
        buffer.data.clear();
    }
    g_clip = Clip{};
    g_active = false;
    g_fading = false;
    g_samples_per_buffer = 0;
    g_next_sample = 0;
    g_clip_position = 0;
    g_fade_start_sample = 0;
    g_fade_samples = 0;
    g_fade_started_tick = 0;
}

}  // namespace

bool ValidateMusicFile(const std::wstring& path, std::wstring* error) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        SetError(error, L"无法打开该音频文件。");
        return false;
    }

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 ||
        static_cast<uint64_t>(size.QuadPart) > kMaxMusicFileBytes) {
        CloseHandle(file);
        SetError(error, L"音频文件需大于 0 且不超过 5 MB。");
        return false;
    }

    const uint64_t file_size = static_cast<uint64_t>(size.QuadPart);
    std::vector<BYTE> bytes(static_cast<size_t>(file_size));
    DWORD read = 0;
    const bool read_ok =
        ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read,
                 nullptr) != FALSE &&
        read == bytes.size();
    CloseHandle(file);
    if (!read_ok) {
        SetError(error, L"读取音频文件失败。");
        return false;
    }

    Clip clip;
    if (!ParseWav(bytes.data(), bytes.size(), &clip)) {
        SetError(error,
                 L"仅支持 PCM WAV（8/16 位、1~2 声道、8~48 kHz）。");
        return false;
    }
    return true;
}

bool StartRestMusic(const std::wstring& user_file) {
    ReleaseDevice();

    Clip clip;
    bool loaded = false;
    if (!user_file.empty()) {
        std::vector<BYTE> bytes;
        if (ReadWholeFile(user_file, &bytes)) {
            loaded = ParseWav(bytes.data(), bytes.size(), &clip);
        }
    }
    if (!loaded) {
        loaded = LoadEmbeddedClip(&clip);
    }
    if (!loaded) {
        return false;
    }

    g_clip = std::move(clip);
    WAVEFORMATEX format = g_clip.format;
    if (waveOutOpen(&g_device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) !=
        MMSYSERR_NOERROR) {
        g_device = nullptr;
        g_clip = Clip{};
        return false;
    }

    g_samples_per_buffer =
        std::max<uint64_t>(1, static_cast<uint64_t>(g_clip.format.nSamplesPerSec) *
                                  kBufferMilliseconds / 1000);
    const size_t buffer_bytes =
        static_cast<size_t>(g_samples_per_buffer * g_clip.block_align);

    for (Buffer& buffer : g_buffers) {
        buffer.data.resize(buffer_bytes);
        buffer.header = {};
        buffer.header.lpData = reinterpret_cast<LPSTR>(buffer.data.data());
        buffer.header.dwBufferLength = static_cast<DWORD>(buffer_bytes);
        if (waveOutPrepareHeader(g_device, &buffer.header, sizeof(WAVEHDR)) !=
            MMSYSERR_NOERROR) {
            ReleaseDevice();
            return false;
        }
    }

    g_active = true;
    g_fading = false;
    g_next_sample = 0;
    g_clip_position = 0;
    g_fade_start_sample = 0;
    g_fade_samples = 0;
    g_fade_started_tick = 0;

    for (Buffer& buffer : g_buffers) {
        if (!QueueBuffer(&buffer)) {
            ReleaseDevice();
            return false;
        }
    }
    return true;
}

void FadeOutRestMusic() {
    if (!g_active || g_fading) {
        return;
    }
    g_fading = true;
    // Buffers already handed to the driver keep their volume; the fade starts
    // with the next sample that has not been queued yet.
    g_fade_start_sample = g_next_sample;
    g_fade_samples = std::max<uint64_t>(
        1, static_cast<uint64_t>(g_clip.format.nSamplesPerSec) *
               kFadeMilliseconds / 1000);
    g_fade_started_tick = GetTickCount64();
}

void StopRestMusic() {
    ReleaseDevice();
}

void ServiceRestMusic() {
    if (!g_active || g_device == nullptr) {
        return;
    }
    if (g_fading && g_fade_started_tick != 0 &&
        GetTickCount64() - g_fade_started_tick >
            kFadeMilliseconds + 2000) {
        // The caller normally stops us at the end of the fade; this is only a
        // safety net so a missed stop cannot leave the device open.
        ReleaseDevice();
        return;
    }

    for (Buffer& buffer : g_buffers) {
        if ((buffer.header.dwFlags & WHDR_DONE) != 0) {
            QueueBuffer(&buffer);
        }
    }
}

bool RestMusicPlaying() {
    return g_active;
}

}  // namespace rr

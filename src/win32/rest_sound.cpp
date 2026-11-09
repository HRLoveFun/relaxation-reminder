#include "rest_sound.h"

#include <windows.h>
#include <mmsystem.h>

#include <cstdint>

#include "core/config.h"

namespace rr {
namespace {

// Resource id of the built-in chime in app.rc (type WAVE).
constexpr int kChimeResourceId = 2;

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

bool ReadAt(HANDLE file, uint64_t offset, void* buffer, DWORD bytes) {
    LARGE_INTEGER position = {};
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(file, position, nullptr, FILE_BEGIN)) {
        return false;
    }
    DWORD read = 0;
    return ReadFile(file, buffer, bytes, &read, nullptr) != FALSE &&
           read == bytes;
}

void SetError(std::wstring* error, const wchar_t* message) {
    if (error != nullptr) {
        *error = message;
    }
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

    unsigned char header[12] = {};
    bool ok = ReadAt(file, 0, header, sizeof(header)) &&
              ChunkIdEquals(header, "RIFF") &&
              ChunkIdEquals(header + 8, "WAVE");
    if (!ok) {
        CloseHandle(file);
        SetError(error, L"仅支持 WAV 格式的音频文件。");
        return false;
    }

    bool have_format = false;
    bool have_data = false;
    uint64_t offset = 12;
    for (int chunk = 0; chunk < 64 && offset + 8 <= file_size; ++chunk) {
        unsigned char chunk_header[8] = {};
        if (!ReadAt(file, offset, chunk_header, sizeof(chunk_header))) {
            break;
        }
        const uint64_t chunk_size = ReadLe32(chunk_header + 4);
        const uint64_t data_offset = offset + 8;
        if (chunk_size > file_size - data_offset) {
            CloseHandle(file);
            SetError(error, L"音频文件已损坏或格式不受支持。");
            return false;
        }

        if (ChunkIdEquals(chunk_header, "fmt ")) {
            if (chunk_size < 16) {
                CloseHandle(file);
                SetError(error, L"音频文件已损坏或格式不受支持。");
                return false;
            }
            unsigned char format[16] = {};
            if (!ReadAt(file, data_offset, format, sizeof(format))) {
                CloseHandle(file);
                SetError(error, L"音频文件已损坏或格式不受支持。");
                return false;
            }
            const uint16_t audio_format = ReadLe16(format);
            const uint16_t channels = ReadLe16(format + 2);
            const uint32_t sample_rate = ReadLe32(format + 4);
            const uint16_t bits_per_sample = ReadLe16(format + 14);
            if (audio_format != 1) {
                CloseHandle(file);
                SetError(error, L"仅支持未压缩的 PCM WAV 音频。");
                return false;
            }
            if (channels < 1 || channels > 2 || sample_rate < 8000 ||
                sample_rate > 48000 ||
                (bits_per_sample != 8 && bits_per_sample != 16)) {
                CloseHandle(file);
                SetError(error, L"音频需为 8/16 位、1~2 声道、8~48 kHz。");
                return false;
            }
            have_format = true;
        } else if (ChunkIdEquals(chunk_header, "data")) {
            if (chunk_size == 0) {
                CloseHandle(file);
                SetError(error, L"音频文件不含声音数据。");
                return false;
            }
            have_data = true;
        }

        // Chunks are word-aligned; odd sizes carry one padding byte.
        offset = data_offset + chunk_size + (chunk_size & 1u);
    }

    CloseHandle(file);
    if (!have_format || !have_data) {
        SetError(error, L"音频文件已损坏或格式不受支持。");
        return false;
    }
    return true;
}

bool PlayRestSound(const std::wstring& user_file) {
    StopRestSound();

    if (!user_file.empty() && ValidateMusicFile(user_file, nullptr)) {
        if (PlaySoundW(user_file.c_str(), nullptr,
                       SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) {
            return true;
        }
    }

    return PlaySoundW(MAKEINTRESOURCEW(kChimeResourceId),
                      GetModuleHandleW(nullptr),
                      SND_RESOURCE | SND_ASYNC | SND_NODEFAULT) != FALSE;
}

void StopRestSound() {
    PlaySoundW(nullptr, nullptr, 0);
}

}  // namespace rr

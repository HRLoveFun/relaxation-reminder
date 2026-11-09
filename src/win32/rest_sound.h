#pragma once

#include <string>

namespace rr {

// Validates a user-selected rest chime. Only PCM WAV is accepted because the
// playback path is PlaySoundW, which does not decode compressed audio.
// Rejects files over kMaxMusicFileBytes and anything that is not
// RIFF/WAVE, PCM, 1-2 channels, 8/16-bit, 8-48 kHz with a non-empty data chunk.
bool ValidateMusicFile(const std::wstring& path, std::wstring* error);

// Plays the user file when it validates, otherwise the embedded chime.
// Playback is asynchronous and stops naturally at the end of the file.
bool PlayRestSound(const std::wstring& user_file);

void StopRestSound();

}  // namespace rr

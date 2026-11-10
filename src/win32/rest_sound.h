#pragma once

#include <string>

namespace rr {

// Validates a user-selected music file. Only PCM WAV is accepted because the
// playback path feeds PCM samples to waveOut. Rejects files over
// kMaxMusicFileBytes and anything that is not RIFF/WAVE, PCM, 1-2 channels,
// 8/16-bit, 8-48 kHz with a non-empty data chunk.
bool ValidateMusicFile(const std::wstring& path, std::wstring* error);

// Starts looping the user file when it validates, otherwise the embedded
// clip. Playback continues until StopRestMusic() or the fade completes.
bool StartRestMusic(const std::wstring& user_file);

// Begins a short volume fade. The fade applies to audio not yet queued, so
// the already-buffered tail keeps its volume; the state machine waits
// kMusicFadeMs before dismissing the window.
void FadeOutRestMusic();

// Stops immediately and releases the audio device.
void StopRestMusic();

// Refills the waveOut queue and advances the fade. Call from the UI tick.
void ServiceRestMusic();

bool RestMusicPlaying();

}  // namespace rr

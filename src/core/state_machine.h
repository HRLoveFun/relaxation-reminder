#pragma once

#include <cstdint>

namespace rr {

enum class State {
    WaitInterval,
    SnoozeWait,
    ShowActive,
    RestRunning,
    RestPaused,
    MusicPlaying,
};

// Fade length used when the user comes back during MusicPlaying or when the
// playback window is about to end. The audio engine fades a little faster, so
// the sound is already silent when the window disappears. Input during music
// fades the sound within one second and closes the window one second later.
inline constexpr uint32_t kMusicFadeMs = 1000;

struct TickResult {
    bool show_reminder = false;
    bool hide_reminder = false;
    bool entered_rest = false;
    bool paused_rest = false;
    bool resumed_rest = false;
    bool final_dismiss = false;
    // Rest countdown reached zero: the window stays open and the caller
    // starts looping the music for the configured playback duration.
    bool music_start = false;
    // Start fading the music now: either the user touched the keyboard/mouse
    // or the playback window is about to end.
    bool music_fade_out = false;
};

// Timing model:
//  WaitInterval --(interval)--> ShowActive --(idle >= threshold)--> RestRunning
//  RestRunning --(input)--> RestPaused --(idle >= threshold)--> RestRunning
//  RestRunning --(rest elapsed)--> MusicPlaying --(play time/input)--> WaitInterval
//  ShowActive/Rest* --(snooze, once)--> SnoozeWait --(snooze)--> ShowActive
//
// Configuration changes are queued and take effect on the next reminder cycle.
// A global pause flag freezes every countdown and the window expansion.
class StateMachine {
public:
    void Configure(uint32_t interval_minutes, uint32_t rest_minutes,
                   uint32_t snooze_minutes, uint32_t idle_threshold_seconds,
                   uint32_t music_play_seconds, bool music_enabled);
    void Start(uint64_t now_ms);

    TickResult Tick(uint64_t now_ms, uint32_t idle_ms);
    bool TrySnooze(uint64_t now_ms);
    void CloseReminder(uint64_t now_ms);

    void OnSuspend(uint64_t now_ms);
    void OnResume(uint64_t now_ms);

    // Test/debug hook: show immediately without waiting for the interval.
    void ForceShow(uint64_t now_ms);

    // Freezes every countdown (interval, snooze, rest, music) and the window
    // expansion until resumed. Manual close still works while paused.
    void SetTimingPaused(bool paused);
    bool timing_paused() const { return timing_paused_; }

    // The music on/off switch takes effect immediately, unlike the queued
    // durations.
    void SetMusicEnabled(bool enabled) { music_enabled_ = enabled; }

    State state() const { return state_; }
    bool IsVisible() const;
    bool CanSnooze() const;
    bool snooze_used() const { return snooze_used_; }
    bool music_fading() const { return music_fading_; }
    uint32_t rest_remaining_seconds() const;
    uint32_t rest_total_seconds() const;
    uint32_t music_remaining_seconds() const;
    uint32_t music_total_seconds() const;
    uint32_t interval_remaining_seconds() const;
    uint32_t idle_threshold_seconds() const;
    uint64_t expansion_elapsed_ms() const { return expansion_elapsed_ms_; }

private:
    void EnterWait(uint64_t now_ms);
    void EnterShow(uint64_t now_ms, bool from_snooze);
    void ApplyPendingInterval();
    void ApplyPendingCycle();

    State state_ = State::WaitInterval;
    bool started_ = false;
    bool suspended_ = false;
    bool timing_paused_ = false;
    bool snooze_used_ = false;
    bool music_fading_ = false;

    uint32_t interval_ms_ = 0;
    uint32_t rest_ms_ = 0;
    uint32_t snooze_ms_ = 0;
    uint32_t idle_ms_ = 0;
    uint32_t music_ms_ = 0;
    bool music_enabled_ = true;

    uint32_t pending_interval_ms_ = 0;
    uint32_t pending_rest_ms_ = 0;
    uint32_t pending_snooze_ms_ = 0;
    uint32_t pending_idle_ms_ = 0;
    uint32_t pending_music_ms_ = 0;

    uint64_t last_tick_ms_ = 0;
    uint64_t remaining_ms_ = 0;
    uint64_t rest_remaining_ms_ = 0;
    uint64_t music_remaining_ms_ = 0;
    uint64_t expansion_elapsed_ms_ = 0;
};

}  // namespace rr

#pragma once

#include <cstdint>

namespace rr {

enum class State {
    WaitInterval,
    SnoozeWait,
    ShowActive,
    RestRunning,
    RestPaused,
};

struct TickResult {
    bool show_reminder = false;
    bool hide_reminder = false;
    bool entered_rest = false;
    bool paused_rest = false;
    bool resumed_rest = false;
    bool final_dismiss = false;
};

// Timing model:
//  WaitInterval --(interval)--> ShowActive --(idle >= threshold)--> RestRunning
//  RestRunning --(input)--> RestPaused --(idle >= threshold)--> RestRunning
//  RestRunning --(rest elapsed)--> WaitInterval
//  ShowActive/Rest* --(snooze, once)--> SnoozeWait --(snooze)--> ShowActive
//
// Configuration changes are queued and take effect on the next reminder cycle.
// A global pause flag freezes every countdown and the window expansion.
class StateMachine {
public:
    void Configure(uint32_t interval_minutes, uint32_t rest_minutes,
                   uint32_t snooze_minutes, uint32_t idle_threshold_seconds);
    void Start(uint64_t now_ms);

    TickResult Tick(uint64_t now_ms, uint32_t idle_ms);
    bool TrySnooze(uint64_t now_ms);
    void CloseReminder(uint64_t now_ms);

    void OnSuspend(uint64_t now_ms);
    void OnResume(uint64_t now_ms);

    // Test/debug hook: show immediately without waiting for the interval.
    void ForceShow(uint64_t now_ms);

    // Freezes every countdown (interval, snooze, rest) and the window
    // expansion until resumed. Manual close/snooze still work while paused.
    void SetTimingPaused(bool paused);
    bool timing_paused() const { return timing_paused_; }

    State state() const { return state_; }
    bool IsVisible() const;
    bool snooze_used() const { return snooze_used_; }
    uint32_t rest_remaining_seconds() const;
    uint32_t rest_total_seconds() const;
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

    uint32_t interval_ms_ = 0;
    uint32_t rest_ms_ = 0;
    uint32_t snooze_ms_ = 0;
    uint32_t idle_ms_ = 0;

    uint32_t pending_interval_ms_ = 0;
    uint32_t pending_rest_ms_ = 0;
    uint32_t pending_snooze_ms_ = 0;
    uint32_t pending_idle_ms_ = 0;

    uint64_t last_tick_ms_ = 0;
    uint64_t remaining_ms_ = 0;
    uint64_t rest_remaining_ms_ = 0;
    uint64_t expansion_elapsed_ms_ = 0;
};

}  // namespace rr

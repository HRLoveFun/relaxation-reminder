#include "state_machine.h"

#include "config.h"

namespace rr {
namespace {

constexpr uint64_t kMinuteMs = 60ull * 1000ull;

uint32_t MinutesToMs(uint32_t minutes) {
    return static_cast<uint32_t>(static_cast<uint64_t>(minutes) * kMinuteMs);
}

uint32_t SecondsCeil(uint64_t ms) {
    return static_cast<uint32_t>((ms + 999ull) / 1000ull);
}

}  // namespace

void StateMachine::Configure(uint32_t interval_minutes, uint32_t rest_minutes,
                             uint32_t snooze_minutes,
                             uint32_t idle_threshold_seconds) {
    pending_interval_ms_ = MinutesToMs(interval_minutes);
    pending_rest_ms_ = MinutesToMs(rest_minutes);
    pending_snooze_ms_ = MinutesToMs(snooze_minutes);
    pending_idle_ms_ = idle_threshold_seconds * 1000u;
}

void StateMachine::Start(uint64_t now_ms) {
    ApplyPendingInterval();
    ApplyPendingCycle();
    started_ = true;
    suspended_ = false;
    last_tick_ms_ = now_ms;
    EnterWait(now_ms);
}

TickResult StateMachine::Tick(uint64_t now_ms, uint32_t idle_ms) {
    TickResult result;

    if (!started_) {
        Start(now_ms);
        return result;
    }
    if (suspended_) {
        last_tick_ms_ = now_ms;
        return result;
    }
    if (timing_paused_) {
        // Keep the clock anchored so resuming continues from the frozen
        // remaining time instead of counting the paused wall time.
        last_tick_ms_ = now_ms;
        return result;
    }

    const uint64_t delta = now_ms > last_tick_ms_ ? now_ms - last_tick_ms_ : 0;
    last_tick_ms_ = now_ms;

    switch (state_) {
        case State::WaitInterval:
            if (delta >= remaining_ms_) {
                EnterShow(now_ms, false);
                result.show_reminder = true;
            } else {
                remaining_ms_ -= delta;
            }
            break;

        case State::SnoozeWait:
            if (delta >= remaining_ms_) {
                EnterShow(now_ms, true);
                result.show_reminder = true;
            } else {
                remaining_ms_ -= delta;
            }
            break;

        case State::ShowActive:
            expansion_elapsed_ms_ += delta;
            if (idle_ms >= idle_ms_) {
                state_ = State::RestRunning;
                rest_remaining_ms_ = rest_ms_;
                result.entered_rest = true;
            }
            break;

        case State::RestRunning:
            // The window keeps expanding on its fixed curve while the user
            // rests; idle state only controls the rest countdown.
            expansion_elapsed_ms_ += delta;
            if (idle_ms < idle_ms_) {
                state_ = State::RestPaused;
                result.paused_rest = true;
            } else if (delta >= rest_remaining_ms_) {
                result.final_dismiss = true;
                result.hide_reminder = true;
                EnterWait(now_ms);
            } else {
                rest_remaining_ms_ -= delta;
            }
            break;

        case State::RestPaused:
            expansion_elapsed_ms_ += delta;
            if (idle_ms >= idle_ms_) {
                state_ = State::RestRunning;
                result.resumed_rest = true;
            }
            break;
    }

    return result;
}

bool StateMachine::TrySnooze(uint64_t now_ms) {
    (void)now_ms;
    if (snooze_used_ || !IsVisible()) {
        return false;
    }

    snooze_used_ = true;
    state_ = State::SnoozeWait;
    remaining_ms_ = snooze_ms_;
    rest_remaining_ms_ = 0;
    expansion_elapsed_ms_ = 0;
    return true;
}

void StateMachine::CloseReminder(uint64_t now_ms) {
    if (IsVisible()) {
        EnterWait(now_ms);
    }
}

void StateMachine::OnSuspend(uint64_t now_ms) {
    last_tick_ms_ = now_ms;
    suspended_ = true;
}

void StateMachine::OnResume(uint64_t now_ms) {
    // Sleep counts toward the reminder interval, but not toward rest or
    // window expansion. Visible states discard the sleep gap.
    if (IsVisible()) {
        last_tick_ms_ = now_ms;
    }
    suspended_ = false;
}

void StateMachine::ForceShow(uint64_t now_ms) {
    Start(now_ms);
    EnterShow(now_ms, false);
}

void StateMachine::SetTimingPaused(bool paused) {
    if (paused == timing_paused_) {
        return;
    }
    timing_paused_ = paused;
}

bool StateMachine::IsVisible() const {
    return state_ == State::ShowActive || state_ == State::RestRunning ||
           state_ == State::RestPaused;
}

uint32_t StateMachine::rest_remaining_seconds() const {
    if (state_ != State::RestRunning && state_ != State::RestPaused) {
        return 0;
    }
    return SecondsCeil(rest_remaining_ms_);
}

uint32_t StateMachine::rest_total_seconds() const {
    if (state_ != State::RestRunning && state_ != State::RestPaused) {
        return 0;
    }
    return SecondsCeil(rest_ms_);
}

uint32_t StateMachine::interval_remaining_seconds() const {
    if (state_ != State::WaitInterval && state_ != State::SnoozeWait) {
        return 0;
    }
    return SecondsCeil(remaining_ms_);
}

uint32_t StateMachine::idle_threshold_seconds() const {
    return idle_ms_ / 1000u;
}

void StateMachine::EnterWait(uint64_t now_ms) {
    (void)now_ms;
    ApplyPendingInterval();
    state_ = State::WaitInterval;
    remaining_ms_ = interval_ms_;
    rest_remaining_ms_ = 0;
    expansion_elapsed_ms_ = 0;
    snooze_used_ = false;
}

void StateMachine::EnterShow(uint64_t now_ms, bool from_snooze) {
    (void)now_ms;
    ApplyPendingCycle();
    state_ = State::ShowActive;
    expansion_elapsed_ms_ = 0;
    rest_remaining_ms_ = 0;
    if (!from_snooze) {
        snooze_used_ = false;
    }
}

void StateMachine::ApplyPendingInterval() {
    interval_ms_ = pending_interval_ms_ != 0
                       ? pending_interval_ms_
                       : MinutesToMs(kDefaultReminderIntervalMinutes);
}

void StateMachine::ApplyPendingCycle() {
    rest_ms_ = pending_rest_ms_ != 0
                   ? pending_rest_ms_
                   : MinutesToMs(kDefaultRestDurationMinutes);
    snooze_ms_ = pending_snooze_ms_ != 0
                     ? pending_snooze_ms_
                     : MinutesToMs(kDefaultSnoozeDurationMinutes);
    idle_ms_ = pending_idle_ms_ != 0
                   ? pending_idle_ms_
                   : kDefaultIdleThresholdSeconds * 1000u;
}

}  // namespace rr

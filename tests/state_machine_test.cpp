#include <cmath>
#include <cstdint>
#include <cstdio>

#include "core/config.h"
#include "core/growth.h"
#include "core/state_machine.h"

namespace {

constexpr uint64_t kSecond = 1000;
constexpr uint64_t kMinute = 60 * kSecond;

int g_failures = 0;

void Check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::printf("FAIL line %d: %s\n", line, expression);
        ++g_failures;
    }
}

#define CHECK(expression) Check((expression), #expression, __LINE__)

rr::StateMachine MakeMachine(uint32_t interval_minutes = 30,
                             uint32_t rest_minutes = 5,
                             uint32_t snooze_minutes = 5,
                             uint32_t idle_seconds = 10) {
    rr::StateMachine machine;
    machine.Configure(interval_minutes, rest_minutes, snooze_minutes,
                      idle_seconds);
    machine.Start(0);
    return machine;
}

void ShowsAfterInterval() {
    rr::StateMachine machine = MakeMachine();

    rr::TickResult result = machine.Tick(30 * kMinute - 1, 0);
    CHECK(!result.show_reminder);
    CHECK(machine.state() == rr::State::WaitInterval);

    result = machine.Tick(30 * kMinute, 0);
    CHECK(result.show_reminder);
    CHECK(machine.state() == rr::State::ShowActive);
    CHECK(machine.IsVisible());
}

void RestStartsOnlyWhenIdle() {
    rr::StateMachine machine = MakeMachine();
    machine.Tick(30 * kMinute, 0);

    rr::TickResult result = machine.Tick(30 * kMinute + 9 * kSecond, 9000);
    CHECK(!result.entered_rest);
    CHECK(machine.state() == rr::State::ShowActive);

    result = machine.Tick(30 * kMinute + 10 * kSecond, 10000);
    CHECK(result.entered_rest);
    CHECK(machine.state() == rr::State::RestRunning);
    CHECK(machine.rest_remaining_seconds() == 300);
}

void RestCountsDownAndDismisses() {
    rr::StateMachine machine = MakeMachine();
    machine.Tick(30 * kMinute, 0);
    uint64_t now = 30 * kMinute;
    machine.Tick(now, 10000);

    for (int second = 0; second < 299; ++second) {
        now += kSecond;
        rr::TickResult result = machine.Tick(now, 10000);
        CHECK(!result.final_dismiss);
    }

    now += kSecond;
    rr::TickResult result = machine.Tick(now, 10000);
    CHECK(result.final_dismiss);
    CHECK(result.hide_reminder);
    CHECK(machine.state() == rr::State::WaitInterval);
    CHECK(machine.interval_remaining_seconds() == 30 * 60);
}

void InputPausesRestAndKeepsRemaining() {
    rr::StateMachine machine = MakeMachine();
    machine.Tick(30 * kMinute, 0);
    uint64_t now = 30 * kMinute;
    machine.Tick(now, 10000);

    now += 60 * kSecond;
    rr::TickResult result = machine.Tick(now, 1000);
    CHECK(result.paused_rest);
    CHECK(machine.state() == rr::State::RestPaused);

    const uint32_t frozen = machine.rest_remaining_seconds();
    for (int i = 0; i < 100; ++i) {
        now += 10 * kSecond;
        result = machine.Tick(now, 1000);
        CHECK(machine.state() == rr::State::RestPaused);
        CHECK(!result.final_dismiss);
    }
    CHECK(machine.rest_remaining_seconds() == frozen);

    now += kSecond;
    result = machine.Tick(now, 10000);
    CHECK(result.resumed_rest);
    CHECK(machine.state() == rr::State::RestRunning);

    now += 5 * kMinute;
    result = machine.Tick(now, 10000);
    CHECK(result.final_dismiss);
}

void SnoozeOnceAndResetsNextCycle() {
    rr::StateMachine machine = MakeMachine();
    machine.Tick(30 * kMinute, 0);
    uint64_t now = 30 * kMinute;

    CHECK(machine.TrySnooze(now));
    CHECK(machine.state() == rr::State::SnoozeWait);
    CHECK(machine.snooze_used());
    CHECK(!machine.TrySnooze(now));

    now += 5 * kMinute;
    rr::TickResult result = machine.Tick(now, 0);
    CHECK(result.show_reminder);
    CHECK(machine.state() == rr::State::ShowActive);
    CHECK(machine.snooze_used());

    machine.CloseReminder(now);
    CHECK(machine.state() == rr::State::WaitInterval);
    CHECK(!machine.snooze_used());
    CHECK(machine.interval_remaining_seconds() == 30 * 60);
}

void ExpansionContinuesWhileResting() {
    rr::StateMachine machine = MakeMachine();
    machine.Tick(30 * kMinute, 0);
    const uint64_t shown_at = 30 * kMinute;

    rr::TickResult result = machine.Tick(shown_at + 10 * kSecond, 10000);
    CHECK(result.entered_rest);
    CHECK(machine.expansion_elapsed_ms() == 10 * kSecond);

    result = machine.Tick(shown_at + 30 * kSecond, 1000);
    CHECK(result.paused_rest);
    CHECK(machine.expansion_elapsed_ms() == 30 * kSecond);

    // Expansion is independent of the idle state: it keeps advancing while
    // the rest countdown is paused.
    result = machine.Tick(shown_at + 60 * kSecond, 1000);
    CHECK(machine.state() == rr::State::RestPaused);
    CHECK(machine.expansion_elapsed_ms() == 60 * kSecond);
}

void PauseFreezesAllCountdowns() {
    rr::StateMachine machine = MakeMachine();
    machine.Tick(10 * kMinute, 0);
    CHECK(machine.interval_remaining_seconds() == 20 * 60);

    machine.SetTimingPaused(true);
    CHECK(machine.timing_paused());
    rr::TickResult result = machine.Tick(30 * kMinute, 0);
    CHECK(!result.show_reminder);
    CHECK(machine.interval_remaining_seconds() == 20 * 60);

    machine.SetTimingPaused(false);
    CHECK(!machine.timing_paused());
    result = machine.Tick(30 * kMinute + 20 * kMinute, 0);
    CHECK(result.show_reminder);
}

void PauseFreezesRestAndExpansion() {
    rr::StateMachine machine = MakeMachine();
    machine.Tick(30 * kMinute, 0);
    machine.Tick(30 * kMinute + 10 * kSecond, 10000);
    CHECK(machine.state() == rr::State::RestRunning);
    const uint32_t remaining = machine.rest_remaining_seconds();

    machine.SetTimingPaused(true);
    rr::TickResult result = machine.Tick(35 * kMinute, 10000);
    CHECK(!result.final_dismiss);
    CHECK(machine.rest_remaining_seconds() == remaining);
    CHECK(machine.expansion_elapsed_ms() == 10 * kSecond);

    machine.SetTimingPaused(false);
    result = machine.Tick(35 * kMinute + 300 * kSecond, 10000);
    CHECK(result.final_dismiss);
    CHECK(machine.state() == rr::State::WaitInterval);
}

void SleepCountsForIntervalOnly() {
    rr::StateMachine interval_machine = MakeMachine();
    interval_machine.Tick(1 * kMinute, 0);
    interval_machine.OnSuspend(1 * kMinute);
    interval_machine.OnResume(31 * kMinute);
    rr::TickResult result = interval_machine.Tick(31 * kMinute, 0);
    CHECK(result.show_reminder);

    rr::StateMachine visible_machine = MakeMachine();
    visible_machine.Tick(30 * kMinute, 0);
    visible_machine.Tick(30 * kMinute + 100, 0);
    const uint64_t before = visible_machine.expansion_elapsed_ms();
    visible_machine.OnSuspend(30 * kMinute + 100);
    visible_machine.OnResume(40 * kMinute);
    visible_machine.Tick(40 * kMinute + 50, 0);
    CHECK(visible_machine.state() == rr::State::ShowActive);
    CHECK(visible_machine.expansion_elapsed_ms() < before + 1000);
}

void CloseReminderEndsCycle() {
    rr::StateMachine machine = MakeMachine();
    machine.Tick(30 * kMinute, 0);
    CHECK(machine.IsVisible());

    machine.CloseReminder(30 * kMinute);
    CHECK(machine.state() == rr::State::WaitInterval);
    CHECK(!machine.IsVisible());
    CHECK(machine.interval_remaining_seconds() == 30 * 60);
}

void ConfigChangesApplyNextCycle() {
    rr::StateMachine machine = MakeMachine();
    machine.Tick(10 * kMinute, 0);

    machine.Configure(5, 10, 2, 5);
    rr::TickResult result = machine.Tick(30 * kMinute, 0);
    CHECK(result.show_reminder);

    machine.CloseReminder(30 * kMinute);
    CHECK(machine.interval_remaining_seconds() == 5 * 60);

    result = machine.Tick(35 * kMinute, 0);
    CHECK(result.show_reminder);
    result = machine.Tick(35 * kMinute + 5 * kSecond, 5000);
    CHECK(result.entered_rest);
    CHECK(machine.rest_remaining_seconds() == 10 * 60);
}

void GrowthReachesCapInSixtySeconds() {
    rr::GrowthCurve curve;
    curve.Reset(320, 1920, 1080);

    const double cap = curve.cap_side();
    CHECK(cap > 780.0 && cap < 800.0);
    CHECK(curve.grows());
    CHECK(std::fabs(curve.SideAt(0) - 320.0) < 1e-9);
    CHECK(std::fabs(curve.SideAt(60000) - cap) < 1e-6);
    CHECK(std::fabs(curve.SideAt(120000) - cap) < 1e-6);

    const double middle = curve.SideAt(30000);
    CHECK(middle > 320.0 && middle < cap);
    CHECK(std::fabs(middle * middle - 320.0 * cap) < 1e-3);
}

void GrowthFitsSmallWorkAreas() {
    rr::GrowthCurve curve;
    curve.Reset(320, 400, 400);
    CHECK(!curve.grows());
    CHECK(std::fabs(curve.cap_side() - 320.0) < 1e-9);
    CHECK(std::fabs(curve.SideAt(60000) - 320.0) < 1e-9);

    curve.Reset(320, 1920, 400);
    CHECK(curve.grows());
    CHECK(curve.cap_side() <= 400.0 + 1e-9);
    CHECK(curve.SideAt(60000) <= 400.0 + 1e-9);
}

void ConfigValidationFallsBack() {
    rr::Config config;
    config.reminder_interval_minutes = 0;
    config.rest_duration_minutes = 1441;
    config.snooze_duration_minutes = 0;
    config.idle_threshold_seconds = 3601;
    config.character_image_file = "..\\escape.png";
    config.user_name = L"  \r\n";

    rr::Config sanitized = rr::Sanitize(config);
    CHECK(sanitized.reminder_interval_minutes == 30);
    CHECK(sanitized.rest_duration_minutes == 5);
    CHECK(sanitized.snooze_duration_minutes == 5);
    CHECK(sanitized.idle_threshold_seconds == 10);
    CHECK(sanitized.character_image_file.empty());
    CHECK(sanitized.autostart);
    CHECK(sanitized.user_name == L"牛马");

    config.user_name = L"  小 明  ";
    sanitized = rr::Sanitize(config);
    CHECK(sanitized.user_name == L"小 明");

    config.user_name = L"A\nB\tC";
    sanitized = rr::Sanitize(config);
    CHECK(sanitized.user_name == L"牛马");

    config.user_name = L"   ";
    sanitized = rr::Sanitize(config);
    CHECK(sanitized.user_name == L"牛马");

    config.user_name = std::wstring(30, L'宝');
    sanitized = rr::Sanitize(config);
    CHECK(sanitized.user_name == std::wstring(20, L'宝'));

    config.reminder_interval_minutes = 1440;
    config.user_name = L"测试";
    config.rest_duration_minutes = 1;
    config.snooze_duration_minutes = 1;
    config.idle_threshold_seconds = 3600;
    config.character_image_file = "character.png";
    sanitized = rr::Sanitize(config);
    CHECK(sanitized.reminder_interval_minutes == 1440);
    CHECK(sanitized.rest_duration_minutes == 1);
    CHECK(sanitized.snooze_duration_minutes == 1);
    CHECK(sanitized.idle_threshold_seconds == 3600);
    CHECK(sanitized.character_image_file == "character.png");
}

}  // namespace

int main() {
    ShowsAfterInterval();
    RestStartsOnlyWhenIdle();
    RestCountsDownAndDismisses();
    InputPausesRestAndKeepsRemaining();
    ExpansionContinuesWhileResting();
    SnoozeOnceAndResetsNextCycle();
    PauseFreezesAllCountdowns();
    PauseFreezesRestAndExpansion();
    SleepCountsForIntervalOnly();
    CloseReminderEndsCycle();
    ConfigChangesApplyNextCycle();
    ConfigValidationFallsBack();
    GrowthReachesCapInSixtySeconds();
    GrowthFitsSmallWorkAreas();

    if (g_failures == 0) {
        std::printf("state_machine_test: all checks passed\n");
        return 0;
    }
    std::printf("state_machine_test: %d check(s) failed\n", g_failures);
    return 1;
}

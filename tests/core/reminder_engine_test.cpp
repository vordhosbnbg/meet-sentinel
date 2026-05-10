#include "core/reminder_engine.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

namespace core = meet_sentinel::core;

namespace {

core::UtcTime at_seconds(long long seconds) {
    return core::UtcTime{std::chrono::seconds{seconds}};
}

core::MeetingInstance meeting(std::string event_id, core::UtcTime starts_at = at_seconds(1'000)) {
    return {
        .id =
            {
                .calendar_id = "primary",
                .event_id = std::move(event_id),
                .instance_id = "2026-05-09T12:00:00Z",
            },
        .title = "Planning",
        .starts_at = starts_at,
        .ends_at = starts_at + 30min,
        .join_url = "https://meet.google.com/example",
    };
}

bool has_trace(const core::ReminderEvaluation& evaluation, const core::ReminderKey& key,
               core::ReminderTraceAction action, core::ReminderReasonCode reason) {
    return std::ranges::any_of(evaluation.traces, [&](const core::ReminderTrace& trace) {
        return trace.key == key && trace.action == action && trace.reason == reason;
    });
}

void emits_starts_soon_inside_window() {
    const std::vector<core::MeetingInstance> meetings{meeting("event-1")};
    const core::ReminderKey expected_key = core::reminder_key(meetings.front(), core::ReminderKind::StartsSoon);

    const auto evaluation = core::evaluate_reminders(meetings, {}, at_seconds(500));

    assert(evaluation.decisions.size() == 1);
    assert(evaluation.decisions.front().key == expected_key);
    assert(evaluation.decisions.front().due_at == at_seconds(400));
    assert(evaluation.decisions.front().reason == core::ReminderReasonCode::DueStartsSoon);
    assert(evaluation.decisions.front().event_source == core::EventDataSource::Fresh);
    assert(
        has_trace(evaluation, expected_key, core::ReminderTraceAction::Emit, core::ReminderReasonCode::DueStartsSoon));
}

void suppresses_fired_reminders_after_restart() {
    const std::vector<core::MeetingInstance> meetings{meeting("event-2")};
    const core::ReminderKey key = core::reminder_key(meetings.front(), core::ReminderKind::StartsSoon);
    const core::ReminderState state{
        .fired_reminders =
            {
                {
                    .key = key,
                    .fired_at = at_seconds(450),
                },
            },
    };

    const auto evaluation = core::evaluate_reminders(meetings, state, at_seconds(500));

    assert(evaluation.decisions.empty());
    assert(has_trace(evaluation, key, core::ReminderTraceAction::Skip, core::ReminderReasonCode::SkippedAlreadyFired));
}

void emits_starts_now_with_late_tolerance() {
    const std::vector<core::MeetingInstance> meetings{meeting("event-3")};
    const core::ReminderKey expected_key = core::reminder_key(meetings.front(), core::ReminderKind::StartsNow);

    const auto evaluation = core::evaluate_reminders(meetings, {}, at_seconds(1'000) + 2min);

    assert(evaluation.decisions.size() == 1);
    assert(evaluation.decisions.front().key == expected_key);
    assert(evaluation.decisions.front().reason == core::ReminderReasonCode::DueStartsNow);
}

void skips_late_polling_after_window() {
    const std::vector<core::MeetingInstance> meetings{meeting("event-4")};
    const core::ReminderKey key = core::reminder_key(meetings.front(), core::ReminderKind::StartsNow);

    const auto evaluation = core::evaluate_reminders(meetings, {}, at_seconds(1'000) + 6min);

    assert(evaluation.decisions.empty());
    assert(has_trace(evaluation, key, core::ReminderTraceAction::Skip, core::ReminderReasonCode::SkippedAfterWindow));
}

void skips_cancelled_meetings_with_trace() {
    std::vector<core::MeetingInstance> meetings{meeting("event-5")};
    meetings.front().cancelled = true;
    const core::ReminderKey key = core::reminder_key(meetings.front(), core::ReminderKind::StartsSoon);

    const auto evaluation = core::evaluate_reminders(meetings, {}, at_seconds(500));

    assert(evaluation.decisions.empty());
    assert(has_trace(evaluation, key, core::ReminderTraceAction::Skip, core::ReminderReasonCode::SkippedCancelled));
}

void skips_dismissed_reminders() {
    const std::vector<core::MeetingInstance> meetings{meeting("event-6")};
    const core::ReminderKey key = core::reminder_key(meetings.front(), core::ReminderKind::StartsSoon);
    const core::ReminderState state{
        .dismissed_reminders =
            {
                {
                    .key = key,
                    .dismissed_at = at_seconds(450),
                },
            },
    };

    const auto evaluation = core::evaluate_reminders(meetings, state, at_seconds(500));

    assert(evaluation.decisions.empty());
    assert(has_trace(evaluation, key, core::ReminderTraceAction::Skip, core::ReminderReasonCode::SkippedDismissed));
}

void snoozed_reminders_wait_until_snooze_time() {
    const std::vector<core::MeetingInstance> meetings{meeting("event-7")};
    const core::ReminderKey key = core::reminder_key(meetings.front(), core::ReminderKind::StartsSoon);
    const core::ReminderState state{
        .snoozed_reminders =
            {
                {
                    .key = key,
                    .snoozed_until = at_seconds(550),
                },
            },
    };

    const auto evaluation = core::evaluate_reminders(meetings, state, at_seconds(500));

    assert(evaluation.decisions.empty());
    assert(has_trace(evaluation, key, core::ReminderTraceAction::Skip, core::ReminderReasonCode::SkippedSnoozed));
}

void snoozed_reminders_emit_when_elapsed_even_if_original_fired() {
    const std::vector<core::MeetingInstance> meetings{meeting("event-8")};
    const core::ReminderKey key = core::reminder_key(meetings.front(), core::ReminderKind::StartsSoon);
    const core::ReminderState state{
        .fired_reminders =
            {
                {
                    .key = key,
                    .fired_at = at_seconds(450),
                },
            },
        .snoozed_reminders =
            {
                {
                    .key = key,
                    .snoozed_until = at_seconds(550),
                },
            },
    };

    const auto evaluation = core::evaluate_reminders(meetings, state, at_seconds(550));

    assert(evaluation.decisions.size() == 1);
    assert(evaluation.decisions.front().key == key);
    assert(evaluation.decisions.front().due_at == at_seconds(550));
    assert(evaluation.decisions.front().reason == core::ReminderReasonCode::DueSnoozeElapsed);
}

void stale_cache_meetings_are_still_eligible() {
    const std::vector<core::MeetingInstance> meetings{meeting("event-9")};

    const auto evaluation =
        core::evaluate_reminders(meetings, {}, at_seconds(500), {}, core::EventDataSource::StaleCache);

    assert(evaluation.decisions.size() == 1);
    assert(evaluation.decisions.front().event_source == core::EventDataSource::StaleCache);
    assert(evaluation.traces.front().event_source == core::EventDataSource::StaleCache);
}

void stable_keys_distinguish_reminder_kinds() {
    const core::MeetingInstance instance = meeting("event-10");

    const core::ReminderKey starts_soon = core::reminder_key(instance, core::ReminderKind::StartsSoon);
    const core::ReminderKey starts_now = core::reminder_key(instance, core::ReminderKind::StartsNow);

    assert(starts_soon.meeting_id == starts_now.meeting_id);
    assert(!(starts_soon == starts_now));
}

} // namespace

int main() {
    emits_starts_soon_inside_window();
    suppresses_fired_reminders_after_restart();
    emits_starts_now_with_late_tolerance();
    skips_late_polling_after_window();
    skips_cancelled_meetings_with_trace();
    skips_dismissed_reminders();
    snoozed_reminders_wait_until_snooze_time();
    snoozed_reminders_emit_when_elapsed_even_if_original_fired();
    stale_cache_meetings_are_still_eligible();
    stable_keys_distinguish_reminder_kinds();
}

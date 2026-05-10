#include "core/reminder_engine.h"

#include <algorithm>

namespace meet_sentinel::core {
namespace {

bool was_fired(std::span<const FiredReminder> fired_reminders, const ReminderKey& key) {
    return std::ranges::any_of(fired_reminders, [&](const FiredReminder& fired) { return fired.key == key; });
}

bool was_dismissed(std::span<const DismissedReminder> dismissed_reminders, const ReminderKey& key) {
    return std::ranges::any_of(dismissed_reminders,
                               [&](const DismissedReminder& dismissed) { return dismissed.key == key; });
}

const SnoozedReminder* find_snooze(std::span<const SnoozedReminder> snoozed_reminders, const ReminderKey& key) {
    const auto snooze =
        std::ranges::find_if(snoozed_reminders, [&](const SnoozedReminder& candidate) { return candidate.key == key; });
    if(snooze == snoozed_reminders.end()) {
        return nullptr;
    }
    return &*snooze;
}

UtcTime due_at_for(const MeetingInstance& meeting, ReminderKind kind, const ReminderEngineConfig& config) {
    switch(kind) {
        case ReminderKind::StartsSoon:
            return meeting.starts_at - config.starts_soon_offset;
        case ReminderKind::StartsNow:
            return meeting.starts_at;
    }

    return meeting.starts_at;
}

void add_trace(ReminderEvaluation& evaluation, const ReminderKey& key, ReminderTraceAction action, UtcTime due_at,
               ReminderReasonCode reason, EventDataSource event_source) {
    evaluation.traces.push_back({
        key,
        action,
        due_at,
        reason,
        event_source,
    });
}

void add_decision(ReminderEvaluation& evaluation, const MeetingInstance& meeting, const ReminderKey& key,
                  UtcTime due_at, ReminderReasonCode reason, EventDataSource event_source) {
    evaluation.decisions.push_back({
        meeting,
        key,
        due_at,
        reason,
        event_source,
    });
    add_trace(evaluation, key, ReminderTraceAction::Emit, due_at, reason, event_source);
}

void skip(ReminderEvaluation& evaluation, const ReminderKey& key, UtcTime due_at, ReminderReasonCode reason,
          EventDataSource event_source) {
    add_trace(evaluation, key, ReminderTraceAction::Skip, due_at, reason, event_source);
}

void evaluate_key(ReminderEvaluation& evaluation, const MeetingInstance& meeting, ReminderKind kind,
                  const ReminderState& state, UtcTime now, const ReminderEngineConfig& config,
                  EventDataSource event_source) {
    const ReminderKey key = reminder_key(meeting, kind);
    const UtcTime normal_due_at = due_at_for(meeting, kind, config);

    if(meeting.cancelled) {
        skip(evaluation, key, normal_due_at, ReminderReasonCode::SkippedCancelled, event_source);
        return;
    }

    if(was_dismissed(state.dismissed_reminders, key)) {
        skip(evaluation, key, normal_due_at, ReminderReasonCode::SkippedDismissed, event_source);
        return;
    }

    if(const SnoozedReminder* snooze = find_snooze(state.snoozed_reminders, key)) {
        if(now < snooze->snoozed_until) {
            skip(evaluation, key, snooze->snoozed_until, ReminderReasonCode::SkippedSnoozed, event_source);
            return;
        }

        add_decision(evaluation, meeting, key, snooze->snoozed_until, ReminderReasonCode::DueSnoozeElapsed,
                     event_source);
        return;
    }

    if(was_fired(state.fired_reminders, key)) {
        skip(evaluation, key, normal_due_at, ReminderReasonCode::SkippedAlreadyFired, event_source);
        return;
    }

    switch(kind) {
        case ReminderKind::StartsSoon:
            if(now < normal_due_at) {
                skip(evaluation, key, normal_due_at, ReminderReasonCode::SkippedBeforeWindow, event_source);
                return;
            }
            if(now >= meeting.starts_at) {
                skip(evaluation, key, normal_due_at, ReminderReasonCode::SkippedAfterWindow, event_source);
                return;
            }
            add_decision(evaluation, meeting, key, normal_due_at, ReminderReasonCode::DueStartsSoon, event_source);
            return;

        case ReminderKind::StartsNow: {
            const auto latest_due_time = meeting.starts_at + config.starts_now_late_window;
            if(now < meeting.starts_at) {
                skip(evaluation, key, normal_due_at, ReminderReasonCode::SkippedBeforeWindow, event_source);
                return;
            }
            if(now > latest_due_time) {
                skip(evaluation, key, normal_due_at, ReminderReasonCode::SkippedAfterWindow, event_source);
                return;
            }
            add_decision(evaluation, meeting, key, normal_due_at, ReminderReasonCode::DueStartsNow, event_source);
            return;
        }
    }
}

} // namespace

ReminderKey reminder_key(const MeetingInstance& meeting, ReminderKind kind) {
    return {
        meeting.id,
        kind,
    };
}

ReminderEvaluation evaluate_reminders(std::span<const MeetingInstance> meetings, const ReminderState& state,
                                      UtcTime now, const ReminderEngineConfig& config, EventDataSource event_source) {
    ReminderEvaluation evaluation;
    for(const MeetingInstance& meeting : meetings) {
        evaluate_key(evaluation, meeting, ReminderKind::StartsSoon, state, now, config, event_source);
        evaluate_key(evaluation, meeting, ReminderKind::StartsNow, state, now, config, event_source);
    }

    return evaluation;
}

std::vector<ReminderDecision> due_reminders(std::span<const MeetingInstance> meetings,
                                            std::span<const FiredReminder> fired_reminders, UtcTime now,
                                            const ReminderEngineConfig& config) {
    ReminderState state;
    state.fired_reminders.assign(fired_reminders.begin(), fired_reminders.end());
    return evaluate_reminders(meetings, state, now, config).decisions;
}

std::string_view to_string(ReminderKind kind) {
    switch(kind) {
        case ReminderKind::StartsSoon:
            return "starts-soon";
        case ReminderKind::StartsNow:
            return "starts-now";
    }

    return "unknown";
}

std::string_view to_string(EventDataSource source) {
    switch(source) {
        case EventDataSource::Fresh:
            return "fresh";
        case EventDataSource::StaleCache:
            return "stale-cache";
    }

    return "unknown";
}

std::string_view to_string(ReminderTraceAction action) {
    switch(action) {
        case ReminderTraceAction::Emit:
            return "emit";
        case ReminderTraceAction::Skip:
            return "skip";
    }

    return "unknown";
}

std::string_view to_string(ReminderReasonCode reason) {
    switch(reason) {
        case ReminderReasonCode::DueStartsSoon:
            return "due-starts-soon";
        case ReminderReasonCode::DueStartsNow:
            return "due-starts-now";
        case ReminderReasonCode::DueSnoozeElapsed:
            return "due-snooze-elapsed";
        case ReminderReasonCode::SkippedCancelled:
            return "skipped-cancelled";
        case ReminderReasonCode::SkippedDismissed:
            return "skipped-dismissed";
        case ReminderReasonCode::SkippedSnoozed:
            return "skipped-snoozed";
        case ReminderReasonCode::SkippedAlreadyFired:
            return "skipped-already-fired";
        case ReminderReasonCode::SkippedBeforeWindow:
            return "skipped-before-window";
        case ReminderReasonCode::SkippedAfterWindow:
            return "skipped-after-window";
    }

    return "unknown";
}

} // namespace meet_sentinel::core

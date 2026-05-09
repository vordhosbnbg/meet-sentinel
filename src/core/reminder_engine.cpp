#include "core/reminder_engine.h"

#include <algorithm>

namespace meet_sentinel::core {
namespace {

bool was_fired(std::span<const FiredReminder> fired_reminders, const std::string& instance_id, ReminderKind kind) {
    return std::ranges::any_of(fired_reminders, [&](const FiredReminder& fired) {
        return fired.instance_id == instance_id && fired.kind == kind;
    });
}

void maybe_add_starts_soon(std::vector<ReminderDecision>& decisions, const MeetingInstance& meeting,
                           std::span<const FiredReminder> fired_reminders, UtcTime now,
                           const ReminderEngineConfig& config) {
    const auto due_at = meeting.starts_at - config.starts_soon_offset;
    if(now < due_at || now >= meeting.starts_at) {
        return;
    }

    if(was_fired(fired_reminders, meeting.instance_id, ReminderKind::StartsSoon)) {
        return;
    }

    decisions.push_back({
        meeting.instance_id,
        ReminderKind::StartsSoon,
        due_at,
        "meeting is inside the starts-soon reminder window and the reminder has not fired",
    });
}

void maybe_add_starts_now(std::vector<ReminderDecision>& decisions, const MeetingInstance& meeting,
                          std::span<const FiredReminder> fired_reminders, UtcTime now,
                          const ReminderEngineConfig& config) {
    const auto latest_due_time = meeting.starts_at + config.starts_now_late_window;
    if(now < meeting.starts_at || now > latest_due_time) {
        return;
    }

    if(was_fired(fired_reminders, meeting.instance_id, ReminderKind::StartsNow)) {
        return;
    }

    decisions.push_back({
        meeting.instance_id,
        ReminderKind::StartsNow,
        meeting.starts_at,
        "meeting has started within the late reminder window and the reminder has not fired",
    });
}

} // namespace

std::vector<ReminderDecision> due_reminders(std::span<const MeetingInstance> meetings,
                                            std::span<const FiredReminder> fired_reminders, UtcTime now,
                                            const ReminderEngineConfig& config) {
    std::vector<ReminderDecision> decisions;

    for(const MeetingInstance& meeting : meetings) {
        if(meeting.cancelled) {
            continue;
        }

        maybe_add_starts_soon(decisions, meeting, fired_reminders, now, config);
        maybe_add_starts_now(decisions, meeting, fired_reminders, now, config);
    }

    return decisions;
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

} // namespace meet_sentinel::core

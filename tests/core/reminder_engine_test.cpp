#include "core/reminder_engine.h"

#include <cassert>
#include <chrono>
#include <vector>

using namespace std::chrono_literals;

namespace {

meet_sentinel::core::UtcTime at_seconds(long long seconds) {
    return meet_sentinel::core::UtcTime{std::chrono::seconds{seconds}};
}

void emits_starts_soon_inside_window() {
    const std::vector<meet_sentinel::core::MeetingInstance> meetings{
        {
            .instance_id = "primary:event-1:2026-05-09T12:00:00Z",
            .title = "Planning",
            .starts_at = at_seconds(1'000),
            .ends_at = at_seconds(1'900),
            .join_url = "https://meet.google.com/example",
        },
    };

    const auto decisions = meet_sentinel::core::due_reminders(meetings, {}, at_seconds(500));

    assert(decisions.size() == 1);
    assert(decisions.front().instance_id == meetings.front().instance_id);
    assert(decisions.front().kind == meet_sentinel::core::ReminderKind::StartsSoon);
}

void suppresses_fired_reminders() {
    const std::vector<meet_sentinel::core::MeetingInstance> meetings{
        {
            .instance_id = "primary:event-2:2026-05-09T12:00:00Z",
            .title = "Review",
            .starts_at = at_seconds(1'000),
            .ends_at = at_seconds(1'900),
        },
    };
    const std::vector<meet_sentinel::core::FiredReminder> fired{
        {
            .instance_id = meetings.front().instance_id,
            .kind = meet_sentinel::core::ReminderKind::StartsSoon,
        },
    };

    const auto decisions = meet_sentinel::core::due_reminders(meetings, fired, at_seconds(500));

    assert(decisions.empty());
}

void emits_starts_now_with_late_tolerance() {
    const std::vector<meet_sentinel::core::MeetingInstance> meetings{
        {
            .instance_id = "primary:event-3:2026-05-09T12:00:00Z",
            .title = "Standup",
            .starts_at = at_seconds(1'000),
            .ends_at = at_seconds(1'900),
        },
    };

    const auto decisions = meet_sentinel::core::due_reminders(meetings, {}, at_seconds(1'000) + 2min);

    assert(decisions.size() == 1);
    assert(decisions.front().kind == meet_sentinel::core::ReminderKind::StartsNow);
}

void skips_cancelled_meetings() {
    const std::vector<meet_sentinel::core::MeetingInstance> meetings{
        {
            .instance_id = "primary:event-4:2026-05-09T12:00:00Z",
            .title = "Cancelled",
            .starts_at = at_seconds(1'000),
            .ends_at = at_seconds(1'900),
            .cancelled = true,
        },
    };

    const auto decisions = meet_sentinel::core::due_reminders(meetings, {}, at_seconds(500));

    assert(decisions.empty());
}

} // namespace

int main() {
    emits_starts_soon_inside_window();
    suppresses_fired_reminders();
    emits_starts_now_with_late_tolerance();
    skips_cancelled_meetings();
}

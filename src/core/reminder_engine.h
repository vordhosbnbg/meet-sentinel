#pragma once

#include <chrono>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace meet_sentinel::core {

using UtcTime = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;

enum class ReminderKind {
    StartsSoon,
    StartsNow,
};

enum class EventDataSource {
    Fresh,
    StaleCache,
};

enum class ReminderTraceAction {
    Emit,
    Skip,
};

enum class ReminderReasonCode {
    DueStartsSoon,
    DueStartsNow,
    DueSnoozeElapsed,
    SkippedCancelled,
    SkippedDismissed,
    SkippedSnoozed,
    SkippedAlreadyFired,
    SkippedBeforeWindow,
    SkippedAfterWindow,
};

struct MeetingInstanceId {
    std::string calendar_id;
    std::string event_id;
    std::string instance_id;

    bool operator==(const MeetingInstanceId&) const = default;
};

struct ReminderKey {
    MeetingInstanceId meeting_id;
    ReminderKind kind;

    bool operator==(const ReminderKey&) const = default;
};

struct MeetingInstance {
    MeetingInstanceId id;
    std::string title;
    UtcTime starts_at;
    UtcTime ends_at;
    std::string join_url;
    bool cancelled = false;
};

struct FiredReminder {
    ReminderKey key;
    UtcTime fired_at;
};

struct DismissedReminder {
    ReminderKey key;
    UtcTime dismissed_at;
};

struct SnoozedReminder {
    ReminderKey key;
    UtcTime snoozed_until;
};

struct ReminderState {
    std::vector<FiredReminder> fired_reminders;
    std::vector<DismissedReminder> dismissed_reminders;
    std::vector<SnoozedReminder> snoozed_reminders;
};

struct ReminderDecision {
    MeetingInstance meeting;
    ReminderKey key;
    UtcTime due_at;
    ReminderReasonCode reason;
    EventDataSource event_source;
};

struct ReminderTrace {
    ReminderKey key;
    ReminderTraceAction action;
    UtcTime due_at;
    ReminderReasonCode reason;
    EventDataSource event_source;
};

struct ReminderEvaluation {
    std::vector<ReminderDecision> decisions;
    std::vector<ReminderTrace> traces;
};

struct ReminderEngineConfig {
    std::chrono::minutes starts_soon_offset{10};
    std::chrono::minutes starts_now_late_window{5};
};

ReminderKey reminder_key(const MeetingInstance& meeting, ReminderKind kind);

ReminderEvaluation evaluate_reminders(std::span<const MeetingInstance> meetings, const ReminderState& state,
                                      UtcTime now, const ReminderEngineConfig& config = {},
                                      EventDataSource event_source = EventDataSource::Fresh);

std::vector<ReminderDecision> due_reminders(std::span<const MeetingInstance> meetings,
                                            std::span<const FiredReminder> fired_reminders, UtcTime now,
                                            const ReminderEngineConfig& config = {});

std::string_view to_string(ReminderKind kind);
std::string_view to_string(EventDataSource source);
std::string_view to_string(ReminderTraceAction action);
std::string_view to_string(ReminderReasonCode reason);

} // namespace meet_sentinel::core

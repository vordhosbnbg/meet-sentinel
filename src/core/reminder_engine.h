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

struct MeetingInstance {
    std::string instance_id;
    std::string title;
    UtcTime starts_at;
    UtcTime ends_at;
    std::string join_url;
    bool cancelled = false;
};

struct FiredReminder {
    std::string instance_id;
    ReminderKind kind;
};

struct ReminderDecision {
    std::string instance_id;
    ReminderKind kind;
    UtcTime due_at;
    std::string reason;
};

struct ReminderEngineConfig {
    std::chrono::minutes starts_soon_offset{10};
    std::chrono::minutes starts_now_late_window{5};
};

std::vector<ReminderDecision> due_reminders(std::span<const MeetingInstance> meetings,
                                            std::span<const FiredReminder> fired_reminders, UtcTime now,
                                            const ReminderEngineConfig& config = {});

std::string_view to_string(ReminderKind kind);

} // namespace meet_sentinel::core

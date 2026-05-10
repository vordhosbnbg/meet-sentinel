#pragma once

#include "core/reminder_engine.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <vector>

namespace meet_sentinel::persistence {

struct Settings {
    int schema_version = 1;
    std::chrono::seconds poll_interval{300};
    std::chrono::seconds stale_cache_tolerance{3600};
    std::chrono::seconds default_snooze_duration{300};
};

struct EventCache {
    int schema_version = 1;
    core::UtcTime cached_at;
    std::vector<core::MeetingInstance> meetings;
};

void save_settings(const Settings& settings, const std::filesystem::path& path);
Settings load_settings(const std::filesystem::path& path);

void save_reminder_state(const core::ReminderState& state, const std::filesystem::path& path);
core::ReminderState load_reminder_state(const std::filesystem::path& path);

void save_event_cache(const EventCache& cache, const std::filesystem::path& path);
std::optional<EventCache> load_event_cache(const std::filesystem::path& path);

} // namespace meet_sentinel::persistence

#include "persistence/json_persistence.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <string>

namespace meet_sentinel::persistence {
namespace {

using Json = nlohmann::json;

long long to_epoch_seconds(core::UtcTime value) {
    return value.time_since_epoch().count();
}

core::UtcTime from_epoch_seconds(long long value) {
    return core::UtcTime{std::chrono::seconds{value}};
}

std::string reminder_kind_to_json(core::ReminderKind kind) {
    return std::string{core::to_string(kind)};
}

core::ReminderKind reminder_kind_from_json(const Json& value) {
    const auto text = value.get<std::string>();
    if(text == core::to_string(core::ReminderKind::StartsSoon)) {
        return core::ReminderKind::StartsSoon;
    }
    if(text == core::to_string(core::ReminderKind::StartsNow)) {
        return core::ReminderKind::StartsNow;
    }
    throw std::runtime_error{"unknown reminder kind: " + text};
}

Json meeting_id_to_json(const core::MeetingInstanceId& id) {
    return Json{
        {"calendar_id", id.calendar_id},
        {"event_id", id.event_id},
        {"instance_id", id.instance_id},
    };
}

core::MeetingInstanceId meeting_id_from_json(const Json& value) {
    return {
        .calendar_id = value.at("calendar_id").get<std::string>(),
        .event_id = value.at("event_id").get<std::string>(),
        .instance_id = value.at("instance_id").get<std::string>(),
    };
}

Json reminder_key_to_json(const core::ReminderKey& key) {
    return Json{
        {"meeting_id", meeting_id_to_json(key.meeting_id)},
        {"kind", reminder_kind_to_json(key.kind)},
    };
}

core::ReminderKey reminder_key_from_json(const Json& value) {
    return {
        .meeting_id = meeting_id_from_json(value.at("meeting_id")),
        .kind = reminder_kind_from_json(value.at("kind")),
    };
}

Json meeting_to_json(const core::MeetingInstance& meeting) {
    return Json{
        {"id", meeting_id_to_json(meeting.id)},
        {"title", meeting.title},
        {"starts_at_epoch_seconds", to_epoch_seconds(meeting.starts_at)},
        {"ends_at_epoch_seconds", to_epoch_seconds(meeting.ends_at)},
        {"join_url", meeting.join_url},
        {"cancelled", meeting.cancelled},
    };
}

core::MeetingInstance meeting_from_json(const Json& value) {
    return {
        .id = meeting_id_from_json(value.at("id")),
        .title = value.value("title", ""),
        .starts_at = from_epoch_seconds(value.at("starts_at_epoch_seconds").get<long long>()),
        .ends_at = from_epoch_seconds(value.at("ends_at_epoch_seconds").get<long long>()),
        .join_url = value.value("join_url", ""),
        .cancelled = value.value("cancelled", false),
    };
}

void ensure_parent_directory(const std::filesystem::path& path) {
    const auto parent = path.parent_path();
    if(!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

void write_json_file(const std::filesystem::path& path, const Json& json) {
    ensure_parent_directory(path);

    auto temporary_path = path;
    temporary_path += ".tmp";

    {
        std::ofstream output{temporary_path, std::ios::binary | std::ios::trunc};
        if(!output) {
            throw std::runtime_error{"failed to open JSON file for writing: " + temporary_path.string()};
        }
        output << json.dump(4) << '\n';
        if(!output) {
            throw std::runtime_error{"failed to write JSON file: " + temporary_path.string()};
        }
    }

    std::error_code rename_error;
    std::filesystem::rename(temporary_path, path, rename_error);
    if(rename_error) {
        std::filesystem::remove(path);
        std::filesystem::rename(temporary_path, path);
    }
}

std::optional<Json> read_json_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if(!input) {
        return std::nullopt;
    }

    Json json;
    input >> json;
    return json;
}

Json settings_to_json(const Settings& settings) {
    return Json{
        {"schema_version", settings.schema_version},
        {"poll_interval_seconds", settings.poll_interval.count()},
        {"stale_cache_tolerance_seconds", settings.stale_cache_tolerance.count()},
        {"default_snooze_seconds", settings.default_snooze_duration.count()},
    };
}

Settings settings_from_json(const Json& value) {
    return {
        .schema_version = value.value("schema_version", 1),
        .poll_interval = std::chrono::seconds{value.value("poll_interval_seconds", 300LL)},
        .stale_cache_tolerance = std::chrono::seconds{value.value("stale_cache_tolerance_seconds", 3600LL)},
        .default_snooze_duration = std::chrono::seconds{value.value("default_snooze_seconds", 300LL)},
    };
}

Json reminder_state_to_json(const core::ReminderState& state) {
    Json fired = Json::array();
    for(const auto& reminder : state.fired_reminders) {
        fired.push_back({
            {"key", reminder_key_to_json(reminder.key)},
            {"fired_at_epoch_seconds", to_epoch_seconds(reminder.fired_at)},
        });
    }

    Json dismissed = Json::array();
    for(const auto& reminder : state.dismissed_reminders) {
        dismissed.push_back({
            {"key", reminder_key_to_json(reminder.key)},
            {"dismissed_at_epoch_seconds", to_epoch_seconds(reminder.dismissed_at)},
        });
    }

    Json snoozed = Json::array();
    for(const auto& reminder : state.snoozed_reminders) {
        snoozed.push_back({
            {"key", reminder_key_to_json(reminder.key)},
            {"snoozed_until_epoch_seconds", to_epoch_seconds(reminder.snoozed_until)},
        });
    }

    return Json{
        {"schema_version", 1},
        {"fired_reminders", fired},
        {"dismissed_reminders", dismissed},
        {"snoozed_reminders", snoozed},
    };
}

core::ReminderState reminder_state_from_json(const Json& value) {
    core::ReminderState state;

    for(const auto& reminder : value.value("fired_reminders", Json::array())) {
        state.fired_reminders.push_back({
            .key = reminder_key_from_json(reminder.at("key")),
            .fired_at = from_epoch_seconds(reminder.at("fired_at_epoch_seconds").get<long long>()),
        });
    }

    for(const auto& reminder : value.value("dismissed_reminders", Json::array())) {
        state.dismissed_reminders.push_back({
            .key = reminder_key_from_json(reminder.at("key")),
            .dismissed_at = from_epoch_seconds(reminder.at("dismissed_at_epoch_seconds").get<long long>()),
        });
    }

    for(const auto& reminder : value.value("snoozed_reminders", Json::array())) {
        state.snoozed_reminders.push_back({
            .key = reminder_key_from_json(reminder.at("key")),
            .snoozed_until = from_epoch_seconds(reminder.at("snoozed_until_epoch_seconds").get<long long>()),
        });
    }

    return state;
}

Json event_cache_to_json(const EventCache& cache) {
    Json meetings = Json::array();
    for(const auto& meeting : cache.meetings) {
        meetings.push_back(meeting_to_json(meeting));
    }

    return Json{
        {"schema_version", cache.schema_version},
        {"cached_at_epoch_seconds", to_epoch_seconds(cache.cached_at)},
        {"meetings", meetings},
    };
}

EventCache event_cache_from_json(const Json& value) {
    EventCache cache{
        .schema_version = value.value("schema_version", 1),
        .cached_at = from_epoch_seconds(value.at("cached_at_epoch_seconds").get<long long>()),
    };

    for(const auto& meeting : value.value("meetings", Json::array())) {
        cache.meetings.push_back(meeting_from_json(meeting));
    }

    return cache;
}

} // namespace

void save_settings(const Settings& settings, const std::filesystem::path& path) {
    write_json_file(path, settings_to_json(settings));
}

Settings load_settings(const std::filesystem::path& path) {
    const auto json = read_json_file(path);
    if(!json) {
        return {};
    }
    return settings_from_json(*json);
}

void save_reminder_state(const core::ReminderState& state, const std::filesystem::path& path) {
    write_json_file(path, reminder_state_to_json(state));
}

core::ReminderState load_reminder_state(const std::filesystem::path& path) {
    const auto json = read_json_file(path);
    if(!json) {
        return {};
    }
    return reminder_state_from_json(*json);
}

void save_event_cache(const EventCache& cache, const std::filesystem::path& path) {
    write_json_file(path, event_cache_to_json(cache));
}

std::optional<EventCache> load_event_cache(const std::filesystem::path& path) {
    const auto json = read_json_file(path);
    if(!json) {
        return std::nullopt;
    }
    return event_cache_from_json(*json);
}

} // namespace meet_sentinel::persistence

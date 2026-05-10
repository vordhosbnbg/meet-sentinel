#include "persistence/file_logger.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace meet_sentinel::persistence {
namespace {

using Json = nlohmann::json;

long long to_epoch_seconds(core::UtcTime value) {
    return value.time_since_epoch().count();
}

Json reminder_key_to_json(const core::ReminderKey& key) {
    return Json{
        {"calendar_id", key.meeting_id.calendar_id},
        {"event_id", key.meeting_id.event_id},
        {"instance_id", key.meeting_id.instance_id},
        {"kind", std::string{core::to_string(key.kind)}},
    };
}

Json fields_to_json(const std::vector<std::pair<std::string, std::string>>& fields) {
    Json json = Json::object();
    for(const auto& [key, value] : fields) {
        json[key] = value;
    }
    return json;
}

Json log_entry_to_json(const LogEntry& entry) {
    return Json{
        {"timestamp_epoch_seconds", to_epoch_seconds(entry.timestamp)},
        {"level", std::string{to_string(entry.level)}},
        {"event_type", std::string{to_string(entry.event_type)}},
        {"message", entry.message},
        {"fields", fields_to_json(entry.fields)},
    };
}

void ensure_parent_directory(const std::filesystem::path& path) {
    const auto parent = path.parent_path();
    if(!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

} // namespace

FileLogger::FileLogger(std::filesystem::path path) : path_{std::move(path)} {}

void FileLogger::append(const LogEntry& entry) const {
    ensure_parent_directory(path_);

    std::ofstream output{path_, std::ios::binary | std::ios::app};
    if(!output) {
        throw std::runtime_error{"failed to open log file for append: " + path_.string()};
    }

    output << log_entry_to_json(entry).dump() << '\n';
    if(!output) {
        throw std::runtime_error{"failed to append log file: " + path_.string()};
    }
}

void FileLogger::log_polling(core::UtcTime timestamp, std::string status, int event_count) const {
    append({
        .timestamp = timestamp,
        .level = LogLevel::Info,
        .event_type = LogEventType::Polling,
        .message = "calendar polling completed",
        .fields =
            {
                {"status", std::move(status)},
                {"event_count", std::to_string(event_count)},
            },
    });
}

void FileLogger::log_normalization(core::UtcTime timestamp, int input_count, int normalized_count) const {
    append({
        .timestamp = timestamp,
        .level = LogLevel::Info,
        .event_type = LogEventType::Normalization,
        .message = "calendar events normalized",
        .fields =
            {
                {"input_count", std::to_string(input_count)},
                {"normalized_count", std::to_string(normalized_count)},
            },
    });
}

void FileLogger::log_reminder_trace(core::UtcTime timestamp, const core::ReminderTrace& trace) const {
    append({
        .timestamp = timestamp,
        .level = LogLevel::Info,
        .event_type = LogEventType::ReminderDecision,
        .message = "reminder decision evaluated",
        .fields =
            {
                {"action", std::string{core::to_string(trace.action)}},
                {"reason", std::string{core::to_string(trace.reason)}},
                {"event_source", std::string{core::to_string(trace.event_source)}},
                {"due_at_epoch_seconds", std::to_string(to_epoch_seconds(trace.due_at))},
                {"key", reminder_key_to_json(trace.key).dump()},
            },
    });
}

void FileLogger::log_stale_state(core::UtcTime timestamp, bool stale, std::string reason) const {
    append({
        .timestamp = timestamp,
        .level = stale ? LogLevel::Warning : LogLevel::Info,
        .event_type = LogEventType::StaleState,
        .message = stale ? "calendar data is stale" : "calendar data is fresh",
        .fields =
            {
                {"stale", stale ? "true" : "false"},
                {"reason", std::move(reason)},
            },
    });
}

void FileLogger::log_auth_state(core::UtcTime timestamp, std::string state, std::string detail) const {
    append({
        .timestamp = timestamp,
        .level = LogLevel::Info,
        .event_type = LogEventType::AuthState,
        .message = "authentication state changed",
        .fields =
            {
                {"state", std::move(state)},
                {"detail", std::move(detail)},
            },
    });
}

void FileLogger::log_error(core::UtcTime timestamp, std::string component, std::string detail) const {
    append({
        .timestamp = timestamp,
        .level = LogLevel::Error,
        .event_type = LogEventType::Error,
        .message = "application error",
        .fields =
            {
                {"component", std::move(component)},
                {"detail", std::move(detail)},
            },
    });
}

const std::filesystem::path& FileLogger::path() const {
    return path_;
}

std::string_view to_string(LogLevel level) {
    switch(level) {
        case LogLevel::Info:
            return "info";
        case LogLevel::Warning:
            return "warning";
        case LogLevel::Error:
            return "error";
    }

    return "unknown";
}

std::string_view to_string(LogEventType event_type) {
    switch(event_type) {
        case LogEventType::Polling:
            return "polling";
        case LogEventType::Normalization:
            return "normalization";
        case LogEventType::ReminderDecision:
            return "reminder-decision";
        case LogEventType::StaleState:
            return "stale-state";
        case LogEventType::AuthState:
            return "auth-state";
        case LogEventType::Error:
            return "error";
    }

    return "unknown";
}

} // namespace meet_sentinel::persistence

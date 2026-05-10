#pragma once

#include "core/reminder_engine.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace meet_sentinel::persistence {

enum class LogLevel {
    Info,
    Warning,
    Error,
};

enum class LogEventType {
    Polling,
    Normalization,
    ReminderDecision,
    StaleState,
    AuthState,
    Error,
};

struct LogEntry {
    core::UtcTime timestamp;
    LogLevel level;
    LogEventType event_type;
    std::string message;
    std::vector<std::pair<std::string, std::string>> fields;
};

class FileLogger {
public:
    explicit FileLogger(std::filesystem::path path);

    void append(const LogEntry& entry) const;
    void log_polling(core::UtcTime timestamp, std::string status, int event_count) const;
    void log_normalization(core::UtcTime timestamp, int input_count, int normalized_count) const;
    void log_reminder_trace(core::UtcTime timestamp, const core::ReminderTrace& trace) const;
    void log_stale_state(core::UtcTime timestamp, bool stale, std::string reason) const;
    void log_auth_state(core::UtcTime timestamp, std::string state, std::string detail) const;
    void log_error(core::UtcTime timestamp, std::string component, std::string detail) const;

    const std::filesystem::path& path() const;

private:
    std::filesystem::path path_;
};

std::string_view to_string(LogLevel level);
std::string_view to_string(LogEventType event_type);

} // namespace meet_sentinel::persistence

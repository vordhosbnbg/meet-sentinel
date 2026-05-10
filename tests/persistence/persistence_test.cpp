#include "persistence/credential_store.h"
#include "persistence/file_logger.h"
#include "persistence/json_persistence.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

namespace core = meet_sentinel::core;
namespace persistence = meet_sentinel::persistence;

namespace {

core::UtcTime at_seconds(long long seconds) {
    return core::UtcTime{std::chrono::seconds{seconds}};
}

std::filesystem::path test_root() {
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("meet-sentinel-persistence-test-" + std::to_string(unique));
}

core::MeetingInstance meeting(std::string event_id = "event-1") {
    return {
        .id =
            {
                .calendar_id = "primary",
                .event_id = std::move(event_id),
                .instance_id = "2026-05-09T12:00:00Z",
            },
        .title = "Planning",
        .starts_at = at_seconds(1'000),
        .ends_at = at_seconds(1'000) + 30min,
        .join_url = "https://meet.google.com/example",
    };
}

std::vector<nlohmann::json> read_json_lines(const std::filesystem::path& path) {
    std::ifstream input{path};
    std::vector<nlohmann::json> lines;
    std::string line;
    while(std::getline(input, line)) {
        lines.push_back(nlohmann::json::parse(line));
    }
    return lines;
}

void settings_round_trip(const std::filesystem::path& root) {
    const persistence::Settings settings{
        .poll_interval = 120s,
        .stale_cache_tolerance = 7200s,
        .default_snooze_duration = 600s,
    };

    const auto path = root / "settings.json";
    persistence::save_settings(settings, path);
    const auto loaded = persistence::load_settings(path);

    assert(loaded.schema_version == 1);
    assert(loaded.poll_interval == 120s);
    assert(loaded.stale_cache_tolerance == 7200s);
    assert(loaded.default_snooze_duration == 600s);
}

void reminder_state_round_trip(const std::filesystem::path& root) {
    const auto instance = meeting();
    const auto starts_soon = core::reminder_key(instance, core::ReminderKind::StartsSoon);
    const auto starts_now = core::reminder_key(instance, core::ReminderKind::StartsNow);
    const core::ReminderState state{
        .fired_reminders =
            {
                {
                    .key = starts_soon,
                    .fired_at = at_seconds(400),
                },
            },
        .dismissed_reminders =
            {
                {
                    .key = starts_now,
                    .dismissed_at = at_seconds(1'001),
                },
            },
        .snoozed_reminders =
            {
                {
                    .key = starts_soon,
                    .snoozed_until = at_seconds(700),
                },
            },
    };

    const auto path = root / "reminders.json";
    persistence::save_reminder_state(state, path);
    const auto loaded = persistence::load_reminder_state(path);

    assert(loaded.fired_reminders.size() == 1);
    assert(loaded.fired_reminders.front().key == starts_soon);
    assert(loaded.fired_reminders.front().fired_at == at_seconds(400));
    assert(loaded.dismissed_reminders.size() == 1);
    assert(loaded.dismissed_reminders.front().key == starts_now);
    assert(loaded.snoozed_reminders.size() == 1);
    assert(loaded.snoozed_reminders.front().snoozed_until == at_seconds(700));
}

void event_cache_round_trip_and_drives_stale_decisions(const std::filesystem::path& root) {
    const persistence::EventCache cache{
        .cached_at = at_seconds(300),
        .meetings = {meeting()},
    };

    const auto path = root / "event-cache.json";
    persistence::save_event_cache(cache, path);
    const auto loaded = persistence::load_event_cache(path);

    assert(loaded.has_value());
    assert(loaded->cached_at == at_seconds(300));
    assert(loaded->meetings.size() == 1);
    assert(loaded->meetings.front().id == cache.meetings.front().id);

    const auto evaluation =
        core::evaluate_reminders(loaded->meetings, {}, at_seconds(500), {}, core::EventDataSource::StaleCache);
    assert(evaluation.decisions.size() == 1);
    assert(evaluation.decisions.front().event_source == core::EventDataSource::StaleCache);
}

void plaintext_credential_store_round_trip(const std::filesystem::path& root) {
    persistence::PlaintextCredentialStore store{root / "credentials.json"};
    const persistence::CredentialRecord credentials{
        .access_token = "access-token",
        .refresh_token = "refresh-token",
        .expires_at = at_seconds(9'999),
        .scopes = {"calendar.readonly"},
    };

    store.save(credentials);
    const auto loaded = store.load();

    assert(store.security() == persistence::CredentialStoreSecurity::PlaintextDevelopmentFallback);
    assert(loaded.has_value());
    assert(loaded->access_token == "access-token");
    assert(loaded->refresh_token == "refresh-token");
    assert(loaded->expires_at == at_seconds(9'999));
    assert(loaded->scopes.size() == 1);

    std::ifstream input{store.path()};
    const auto json = nlohmann::json::parse(input);
    assert(json.at("development_plaintext_credentials").get<bool>());

    store.clear();
    assert(!store.load().has_value());
}

void file_logger_records_required_event_types(const std::filesystem::path& root) {
    const persistence::FileLogger logger{root / "meet-sentinel.log"};
    const auto instance = meeting();
    const core::ReminderTrace trace{
        .key = core::reminder_key(instance, core::ReminderKind::StartsSoon),
        .action = core::ReminderTraceAction::Emit,
        .due_at = at_seconds(400),
        .reason = core::ReminderReasonCode::DueStartsSoon,
        .event_source = core::EventDataSource::Fresh,
    };

    logger.log_polling(at_seconds(1), "ok", 2);
    logger.log_normalization(at_seconds(2), 2, 1);
    logger.log_reminder_trace(at_seconds(3), trace);
    logger.log_stale_state(at_seconds(4), true, "network-error");
    logger.log_auth_state(at_seconds(5), "authenticated", "token-loaded");
    logger.log_error(at_seconds(6), "calendar", "timeout");

    const auto lines = read_json_lines(logger.path());
    std::set<std::string> event_types;
    for(const auto& line : lines) {
        event_types.insert(line.at("event_type").get<std::string>());
    }

    assert(lines.size() == 6);
    assert(event_types.contains("polling"));
    assert(event_types.contains("normalization"));
    assert(event_types.contains("reminder-decision"));
    assert(event_types.contains("stale-state"));
    assert(event_types.contains("auth-state"));
    assert(event_types.contains("error"));
}

} // namespace

int main() {
    const auto root = test_root();
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    settings_round_trip(root);
    reminder_state_round_trip(root);
    event_cache_round_trip_and_drives_stale_decisions(root);
    plaintext_credential_store_round_trip(root);
    file_logger_records_required_event_types(root);

    std::filesystem::remove_all(root);
}

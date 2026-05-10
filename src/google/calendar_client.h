#pragma once

#include "google/http_client.h"

#include <optional>
#include <string>

namespace meet_sentinel::google {

struct CalendarEventsRequest {
    std::string access_token;
    std::string calendar_id = "primary";
    std::string time_min_rfc3339;
    std::string time_max_rfc3339;
    std::optional<std::string> page_token;
    int max_results = 50;
};

struct CalendarEventsResponse {
    std::string raw_json;
    long status_code = 0;
};

class CalendarClient {
public:
    explicit CalendarClient(HttpClient& http_client);

    CalendarEventsResponse fetch_events(const CalendarEventsRequest& request) const;

private:
    HttpClient& http_client_;
};

} // namespace meet_sentinel::google

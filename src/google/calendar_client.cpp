#include "google/calendar_client.h"

#include "google/url_encoding.h"

#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

namespace meet_sentinel::google {
namespace {

std::string calendar_events_url(const CalendarEventsRequest& request) {
    std::vector<FormField> query{
        {"singleEvents", "true"},
        {"orderBy", "startTime"},
        {"maxResults", std::to_string(request.max_results)},
    };

    if(!request.time_min_rfc3339.empty()) {
        query.push_back({"timeMin", request.time_min_rfc3339});
    }
    if(!request.time_max_rfc3339.empty()) {
        query.push_back({"timeMax", request.time_max_rfc3339});
    }
    if(request.page_token.has_value()) {
        query.push_back({"pageToken", *request.page_token});
    }

    return "https://www.googleapis.com/calendar/v3/calendars/" + percent_encode(request.calendar_id) + "/events?" +
           form_encode(query);
}

} // namespace

CalendarClient::CalendarClient(HttpClient& http_client) : http_client_{http_client} {}

CalendarEventsResponse CalendarClient::fetch_events(const CalendarEventsRequest& request) const {
    if(request.access_token.empty()) {
        throw HttpError{HttpErrorCode::InvalidRequest, "Calendar request access token is empty"};
    }

    const HttpResponse response = http_client_.send({
        .method = "GET",
        .url = calendar_events_url(request),
        .headers =
            {
                {"Authorization", "Bearer " + request.access_token},
                {"Accept", "application/json"},
            },
        .timeout = std::chrono::seconds{30},
    });

    if(response.status_code < 200 || response.status_code >= 300) {
        throw HttpError{HttpErrorCode::Transport,
                        "Google Calendar endpoint returned HTTP " + std::to_string(response.status_code)};
    }

    return {
        .raw_json = response.body,
        .status_code = response.status_code,
    };
}

} // namespace meet_sentinel::google

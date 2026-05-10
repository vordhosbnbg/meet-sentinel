#include "google/calendar_client.h"
#include "google/curl_http_client.h"
#include "google/http_client.h"
#include "google/loopback_authorization_server.h"
#include "google/oauth_client.h"
#include "google/pkce.h"
#include "google/url_encoding.h"

#include <cassert>
#include <chrono>
#include <future>
#include <string>
#include <vector>

namespace google = meet_sentinel::google;

namespace {

class FakeHttpClient final : public google::HttpClient {
public:
    google::HttpResponse response;
    google::HttpRequest last_request;
    int calls = 0;

    google::HttpResponse send(const google::HttpRequest& request) override {
        last_request = request;
        ++calls;
        return response;
    }
};

bool has_header(const google::HttpRequest& request, const std::string& name, const std::string& value) {
    for(const auto& header : request.headers) {
        if(header.name == name && header.value == value) {
            return true;
        }
    }
    return false;
}

void percent_encoding_is_rfc3986_style() {
    assert(google::percent_encode("abc-._~") == "abc-._~");
    assert(google::percent_encode("calendar id@example.com") == "calendar%20id%40example.com");
}

void pkce_matches_rfc7636_example() {
    const std::string verifier = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
    const std::string expected = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM";

    assert(google::pkce_challenge_for_verifier(verifier) == expected);
}

void authorization_url_contains_pkce_parameters() {
    FakeHttpClient http;
    const google::OAuthClient oauth{http};
    const std::string url = oauth.authorization_url({
        .config =
            {
                .client_id = "client-id",
                .redirect_uri = "http://127.0.0.1:49152/callback",
                .scopes = {"https://www.googleapis.com/auth/calendar.readonly"},
            },
        .state = "state-value",
        .code_challenge = "challenge-value",
    });

    assert(url.starts_with("https://accounts.google.com/o/oauth2/v2/auth?"));
    assert(url.find("client_id=client-id") != std::string::npos);
    assert(url.find("response_type=code") != std::string::npos);
    assert(url.find("code_challenge=challenge-value") != std::string::npos);
    assert(url.find("code_challenge_method=S256") != std::string::npos);
    assert(url.find("access_type=offline") != std::string::npos);
}

void token_exchange_posts_form_body() {
    FakeHttpClient http;
    http.response = {
        .status_code = 200,
        .headers = {{"Content-Type", "application/json"}},
        .body =
            R"({"access_token":"access","refresh_token":"refresh","token_type":"Bearer","expires_in":3600,"scope":"calendar.readonly"})",
    };

    const google::OAuthClient oauth{http};
    const auto token = oauth.exchange_code({
        .config =
            {
                .client_id = "client-id",
                .redirect_uri = "http://127.0.0.1:49152/callback",
            },
        .authorization_code = "auth-code",
        .code_verifier = "verifier",
    });

    assert(http.calls == 1);
    assert(http.last_request.method == "POST");
    assert(http.last_request.url == "https://oauth2.googleapis.com/token");
    assert(has_header(http.last_request, "Content-Type", "application/x-www-form-urlencoded"));
    assert(http.last_request.body.find("grant_type=authorization_code") != std::string::npos);
    assert(http.last_request.body.find("code=auth-code") != std::string::npos);
    assert(http.last_request.body.find("code_verifier=verifier") != std::string::npos);
    assert(token.access_token == "access");
    assert(token.refresh_token == "refresh");
    assert(token.expires_in == std::chrono::seconds{3600});
    assert(token.scopes.size() == 1);
}

void calendar_client_fetches_primary_events() {
    FakeHttpClient http;
    http.response = {
        .status_code = 200,
        .headers = {{"Content-Type", "application/json"}},
        .body = R"({"items":[]})",
    };

    const google::CalendarClient calendar{http};
    const auto response = calendar.fetch_events({
        .access_token = "access-token",
        .time_min_rfc3339 = "2026-05-10T00:00:00Z",
        .time_max_rfc3339 = "2026-05-11T00:00:00Z",
    });

    assert(http.calls == 1);
    assert(http.last_request.method == "GET");
    assert(http.last_request.url.starts_with("https://www.googleapis.com/calendar/v3/calendars/primary/events?"));
    assert(http.last_request.url.find("singleEvents=true") != std::string::npos);
    assert(http.last_request.url.find("orderBy=startTime") != std::string::npos);
    assert(http.last_request.url.find("timeMin=2026-05-10T00%3A00%3A00Z") != std::string::npos);
    assert(has_header(http.last_request, "Authorization", "Bearer access-token"));
    assert(response.raw_json == R"({"items":[]})");
}

void loopback_server_receives_oauth_callback() {
    google::LoopbackAuthorizationServer server;
    auto callback_future =
        std::async(std::launch::async, [&server] { return server.wait_for_callback(std::chrono::seconds{5}); });

    google::CurlHttpClient http;
    const google::HttpResponse response = http.send({
        .method = "GET",
        .url = server.redirect_uri() + "?code=auth%20code&state=state-value",
        .timeout = std::chrono::seconds{5},
    });

    const auto callback = callback_future.get();

    assert(response.status_code == 200);
    assert(callback.code == "auth code");
    assert(callback.state == "state-value");
    assert(callback.error.empty());
}

} // namespace

int main() {
    percent_encoding_is_rfc3986_style();
    pkce_matches_rfc7636_example();
    authorization_url_contains_pkce_parameters();
    token_exchange_posts_form_body();
    calendar_client_fetches_primary_events();
    loopback_server_receives_oauth_callback();
}

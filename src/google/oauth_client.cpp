#include "google/oauth_client.h"

#include "google/url_encoding.h"

#include <nlohmann/json.hpp>

#include <sstream>
#include <stdexcept>
#include <string>

namespace meet_sentinel::google {
namespace {

constexpr std::string_view kAuthorizationEndpoint = "https://accounts.google.com/o/oauth2/v2/auth";
constexpr std::string_view kTokenEndpoint = "https://oauth2.googleapis.com/token";

std::string join_scopes(const std::vector<std::string>& scopes) {
    std::string joined;
    for(const auto& scope : scopes) {
        if(!joined.empty()) {
            joined.push_back(' ');
        }
        joined += scope;
    }
    return joined;
}

OAuthTokenResponse parse_token_response(const HttpResponse& response) {
    if(response.status_code < 200 || response.status_code >= 300) {
        throw HttpError{HttpErrorCode::Transport,
                        "OAuth token endpoint returned HTTP " + std::to_string(response.status_code)};
    }

    const auto json = nlohmann::json::parse(response.body);
    OAuthTokenResponse token{
        .access_token = json.at("access_token").get<std::string>(),
        .refresh_token = json.value("refresh_token", ""),
        .token_type = json.value("token_type", ""),
        .expires_in = std::chrono::seconds{json.value("expires_in", 0LL)},
    };

    std::istringstream scopes{json.value("scope", "")};
    std::string scope;
    while(scopes >> scope) {
        token.scopes.push_back(scope);
    }

    return token;
}

HttpRequest token_request(std::vector<FormField> fields) {
    return {
        .method = "POST",
        .url = std::string{kTokenEndpoint},
        .headers =
            {
                {"Content-Type", "application/x-www-form-urlencoded"},
                {"Accept", "application/json"},
            },
        .body = form_encode(fields),
        .timeout = std::chrono::seconds{30},
    };
}

} // namespace

OAuthClient::OAuthClient(HttpClient& http_client) : http_client_{http_client} {}

std::string OAuthClient::authorization_url(const AuthorizationUrlRequest& request) const {
    const std::vector<FormField> fields{
        {"client_id", request.config.client_id},
        {"redirect_uri", request.config.redirect_uri},
        {"response_type", "code"},
        {"scope", join_scopes(request.config.scopes)},
        {"state", request.state},
        {"code_challenge", request.code_challenge},
        {"code_challenge_method", request.code_challenge_method},
        {"access_type", "offline"},
        {"prompt", "consent"},
    };

    return std::string{kAuthorizationEndpoint} + "?" + form_encode(fields);
}

OAuthTokenResponse OAuthClient::exchange_code(const TokenExchangeRequest& request) const {
    const HttpResponse response = http_client_.send(token_request({
        {"client_id", request.config.client_id},
        {"code", request.authorization_code},
        {"code_verifier", request.code_verifier},
        {"redirect_uri", request.config.redirect_uri},
        {"grant_type", "authorization_code"},
    }));

    return parse_token_response(response);
}

OAuthTokenResponse OAuthClient::refresh_access_token(const TokenRefreshRequest& request) const {
    const HttpResponse response = http_client_.send(token_request({
        {"client_id", request.client_id},
        {"refresh_token", request.refresh_token},
        {"grant_type", "refresh_token"},
    }));

    return parse_token_response(response);
}

} // namespace meet_sentinel::google

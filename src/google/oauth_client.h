#pragma once

#include "google/http_client.h"

#include <chrono>
#include <string>
#include <vector>

namespace meet_sentinel::google {

struct OAuthConfig {
    std::string client_id;
    std::string redirect_uri;
    std::vector<std::string> scopes;
};

struct AuthorizationUrlRequest {
    OAuthConfig config;
    std::string state;
    std::string code_challenge;
    std::string code_challenge_method = "S256";
};

struct TokenExchangeRequest {
    OAuthConfig config;
    std::string authorization_code;
    std::string code_verifier;
};

struct TokenRefreshRequest {
    std::string client_id;
    std::string refresh_token;
};

struct OAuthTokenResponse {
    std::string access_token;
    std::string refresh_token;
    std::string token_type;
    std::chrono::seconds expires_in{0};
    std::vector<std::string> scopes;
};

class OAuthClient {
public:
    explicit OAuthClient(HttpClient& http_client);

    std::string authorization_url(const AuthorizationUrlRequest& request) const;
    OAuthTokenResponse exchange_code(const TokenExchangeRequest& request) const;
    OAuthTokenResponse refresh_access_token(const TokenRefreshRequest& request) const;

private:
    HttpClient& http_client_;
};

} // namespace meet_sentinel::google

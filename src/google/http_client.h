#pragma once

#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace meet_sentinel::google {

struct HttpHeader {
    std::string name;
    std::string value;
};

struct HttpRequest {
    std::string method = "GET";
    std::string url;
    std::vector<HttpHeader> headers;
    std::string body;
    std::chrono::seconds timeout{30};
};

struct HttpResponse {
    long status_code = 0;
    std::vector<HttpHeader> headers;
    std::string body;
};

enum class HttpErrorCode {
    Transport,
    Timeout,
    InvalidRequest,
};

class HttpError final : public std::runtime_error {
public:
    HttpError(HttpErrorCode code, std::string message);

    HttpErrorCode code() const;

private:
    HttpErrorCode code_;
};

class HttpClient {
public:
    virtual ~HttpClient() = default;

    virtual HttpResponse send(const HttpRequest& request) = 0;
};

std::string_view to_string(HttpErrorCode code);

} // namespace meet_sentinel::google

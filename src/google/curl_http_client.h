#pragma once

#include "google/http_client.h"

namespace meet_sentinel::google {

class CurlHttpClient final : public HttpClient {
public:
    CurlHttpClient();
    ~CurlHttpClient() override;

    CurlHttpClient(const CurlHttpClient&) = delete;
    CurlHttpClient& operator=(const CurlHttpClient&) = delete;

    HttpResponse send(const HttpRequest& request) override;
};

} // namespace meet_sentinel::google

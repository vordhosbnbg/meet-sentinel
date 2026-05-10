#include "google/curl_http_client.h"

#include <curl/curl.h>

#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace meet_sentinel::google {
namespace {

class CurlGlobal {
public:
    CurlGlobal() {
        const CURLcode code = curl_global_init(CURL_GLOBAL_DEFAULT);
        if(code != CURLE_OK) {
            throw HttpError{HttpErrorCode::Transport, "curl_global_init failed"};
        }
    }

    ~CurlGlobal() {
        curl_global_cleanup();
    }
};

void ensure_curl_global() {
    static CurlGlobal global;
}

struct CurlSlistDeleter {
    void operator()(curl_slist* value) const {
        curl_slist_free_all(value);
    }
};

std::string trim(std::string_view value) {
    auto begin = value.begin();
    auto end = value.end();

    while(begin != end && std::isspace(static_cast<unsigned char>(*begin)) != 0) {
        ++begin;
    }
    while(begin != end && std::isspace(static_cast<unsigned char>(*(end - 1))) != 0) {
        --end;
    }

    return std::string{begin, end};
}

size_t write_body(char* data, size_t size, size_t count, void* user_data) {
    auto& body = *static_cast<std::string*>(user_data);
    body.append(data, size * count);
    return size * count;
}

size_t write_header(char* data, size_t size, size_t count, void* user_data) {
    const std::string_view line{data, size * count};
    auto& headers = *static_cast<std::vector<HttpHeader>*>(user_data);

    const auto separator = line.find(':');
    if(separator == std::string_view::npos) {
        return size * count;
    }

    headers.push_back({
        trim(line.substr(0, separator)),
        trim(line.substr(separator + 1)),
    });
    return size * count;
}

curl_slist* append_headers(curl_slist* list, const std::vector<HttpHeader>& headers) {
    for(const HttpHeader& header : headers) {
        const std::string line = header.name + ": " + header.value;
        curl_slist* updated = curl_slist_append(list, line.c_str());
        if(updated == nullptr) {
            curl_slist_free_all(list);
            throw HttpError{HttpErrorCode::Transport, "failed to allocate curl header list"};
        }
        list = updated;
    }

    return list;
}

HttpErrorCode error_code_for(CURLcode code) {
    if(code == CURLE_OPERATION_TIMEDOUT) {
        return HttpErrorCode::Timeout;
    }
    return HttpErrorCode::Transport;
}

void set_request_method(CURL* curl, const HttpRequest& request) {
    if(request.method == "GET") {
        return;
    }

    if(request.method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
}

} // namespace

CurlHttpClient::CurlHttpClient() {
    ensure_curl_global();
}

CurlHttpClient::~CurlHttpClient() = default;

HttpResponse CurlHttpClient::send(const HttpRequest& request) {
    if(request.url.empty()) {
        throw HttpError{HttpErrorCode::InvalidRequest, "HTTP request URL is empty"};
    }

    CURL* curl = curl_easy_init();
    if(curl == nullptr) {
        throw HttpError{HttpErrorCode::Transport, "curl_easy_init failed"};
    }

    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl_guard{curl, curl_easy_cleanup};
    std::unique_ptr<curl_slist, CurlSlistDeleter> header_list{append_headers(nullptr, request.headers)};

    HttpResponse response;
    char error_buffer[CURL_ERROR_SIZE] = {};

    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list.get());
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(request.timeout.count() * 1000));
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "meet-sentinel/0.1");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);

    set_request_method(curl, request);

    const CURLcode code = curl_easy_perform(curl);
    if(code != CURLE_OK) {
        const std::string detail = error_buffer[0] != '\0' ? error_buffer : curl_easy_strerror(code);
        throw HttpError{error_code_for(code), detail};
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
    return response;
}

} // namespace meet_sentinel::google

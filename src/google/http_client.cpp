#include "google/http_client.h"

#include <utility>

namespace meet_sentinel::google {

HttpError::HttpError(HttpErrorCode code, std::string message) : std::runtime_error{std::move(message)}, code_{code} {}

HttpErrorCode HttpError::code() const {
    return code_;
}

std::string_view to_string(HttpErrorCode code) {
    switch(code) {
        case HttpErrorCode::Transport:
            return "transport";
        case HttpErrorCode::Timeout:
            return "timeout";
        case HttpErrorCode::InvalidRequest:
            return "invalid-request";
    }

    return "unknown";
}

} // namespace meet_sentinel::google

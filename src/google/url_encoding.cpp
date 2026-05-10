#include "google/url_encoding.h"

#include <array>

namespace meet_sentinel::google {
namespace {

bool is_unreserved(unsigned char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') ||
           value == '-' || value == '.' || value == '_' || value == '~';
}

} // namespace

std::string percent_encode(std::string_view value) {
    constexpr std::array<char, 16> hex{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    std::string encoded;
    for(const unsigned char byte : value) {
        if(is_unreserved(byte)) {
            encoded.push_back(static_cast<char>(byte));
            continue;
        }

        encoded.push_back('%');
        encoded.push_back(hex[(byte >> 4U) & 0x0FU]);
        encoded.push_back(hex[byte & 0x0FU]);
    }

    return encoded;
}

std::string form_encode(const std::vector<FormField>& fields) {
    std::string encoded;
    for(const FormField& field : fields) {
        if(!encoded.empty()) {
            encoded.push_back('&');
        }
        encoded += percent_encode(field.name);
        encoded.push_back('=');
        encoded += percent_encode(field.value);
    }

    return encoded;
}

} // namespace meet_sentinel::google

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace meet_sentinel::google {

struct FormField {
    std::string name;
    std::string value;
};

std::string percent_encode(std::string_view value);
std::string form_encode(const std::vector<FormField>& fields);

} // namespace meet_sentinel::google

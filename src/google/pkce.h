#pragma once

#include <string>
#include <string_view>

namespace meet_sentinel::google {

struct PkceChallenge {
    std::string verifier;
    std::string challenge;
    std::string method = "S256";
};

PkceChallenge generate_pkce_challenge();
std::string pkce_challenge_for_verifier(std::string_view verifier);

} // namespace meet_sentinel::google

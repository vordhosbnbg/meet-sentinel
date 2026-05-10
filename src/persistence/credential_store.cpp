#include "persistence/credential_store.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <utility>

namespace meet_sentinel::persistence {
namespace {

using Json = nlohmann::json;

long long to_epoch_seconds(core::UtcTime value) {
    return value.time_since_epoch().count();
}

core::UtcTime from_epoch_seconds(long long value) {
    return core::UtcTime{std::chrono::seconds{value}};
}

void ensure_parent_directory(const std::filesystem::path& path) {
    const auto parent = path.parent_path();
    if(!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

void write_json_file(const std::filesystem::path& path, const Json& json) {
    ensure_parent_directory(path);

    auto temporary_path = path;
    temporary_path += ".tmp";

    {
        std::ofstream output{temporary_path, std::ios::binary | std::ios::trunc};
        if(!output) {
            throw std::runtime_error{"failed to open credential file for writing: " + temporary_path.string()};
        }
        output << json.dump(4) << '\n';
        if(!output) {
            throw std::runtime_error{"failed to write credential file: " + temporary_path.string()};
        }
    }

    std::error_code rename_error;
    std::filesystem::rename(temporary_path, path, rename_error);
    if(rename_error) {
        std::filesystem::remove(path);
        std::filesystem::rename(temporary_path, path);
    }
}

std::optional<Json> read_json_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if(!input) {
        return std::nullopt;
    }

    Json json;
    input >> json;
    return json;
}

Json credential_to_json(const CredentialRecord& credentials) {
    return Json{
        {"schema_version", credentials.schema_version},
        {"development_plaintext_credentials", true},
        {"access_token", credentials.access_token},
        {"refresh_token", credentials.refresh_token},
        {"token_type", credentials.token_type},
        {"expires_at_epoch_seconds", to_epoch_seconds(credentials.expires_at)},
        {"scopes", credentials.scopes},
    };
}

CredentialRecord credential_from_json(const Json& value) {
    return {
        .schema_version = value.value("schema_version", 1),
        .access_token = value.value("access_token", ""),
        .refresh_token = value.value("refresh_token", ""),
        .token_type = value.value("token_type", "Bearer"),
        .expires_at = from_epoch_seconds(value.at("expires_at_epoch_seconds").get<long long>()),
        .scopes = value.value("scopes", std::vector<std::string>{}),
    };
}

} // namespace

PlaintextCredentialStore::PlaintextCredentialStore(std::filesystem::path path) : path_{std::move(path)} {}

void PlaintextCredentialStore::save(const CredentialRecord& credentials) {
    write_json_file(path_, credential_to_json(credentials));
}

std::optional<CredentialRecord> PlaintextCredentialStore::load() const {
    const auto json = read_json_file(path_);
    if(!json) {
        return std::nullopt;
    }
    return credential_from_json(*json);
}

void PlaintextCredentialStore::clear() {
    std::filesystem::remove(path_);
}

CredentialStoreSecurity PlaintextCredentialStore::security() const {
    return CredentialStoreSecurity::PlaintextDevelopmentFallback;
}

const std::filesystem::path& PlaintextCredentialStore::path() const {
    return path_;
}

std::string_view to_string(CredentialStoreSecurity security) {
    switch(security) {
        case CredentialStoreSecurity::SecurePreferred:
            return "secure-preferred";
        case CredentialStoreSecurity::PlaintextDevelopmentFallback:
            return "plaintext-development-fallback";
    }

    return "unknown";
}

} // namespace meet_sentinel::persistence

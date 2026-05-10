#pragma once

#include "core/reminder_engine.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace meet_sentinel::persistence {

enum class CredentialStoreSecurity {
    SecurePreferred,
    PlaintextDevelopmentFallback,
};

struct CredentialRecord {
    int schema_version = 1;
    std::string access_token;
    std::string refresh_token;
    std::string token_type = "Bearer";
    core::UtcTime expires_at;
    std::vector<std::string> scopes;
};

class CredentialStore {
public:
    virtual ~CredentialStore() = default;

    virtual void save(const CredentialRecord& credentials) = 0;
    virtual std::optional<CredentialRecord> load() const = 0;
    virtual void clear() = 0;
    virtual CredentialStoreSecurity security() const = 0;
};

class PlaintextCredentialStore final : public CredentialStore {
public:
    explicit PlaintextCredentialStore(std::filesystem::path path);

    void save(const CredentialRecord& credentials) override;
    std::optional<CredentialRecord> load() const override;
    void clear() override;
    CredentialStoreSecurity security() const override;

    const std::filesystem::path& path() const;

private:
    std::filesystem::path path_;
};

std::string_view to_string(CredentialStoreSecurity security);

} // namespace meet_sentinel::persistence

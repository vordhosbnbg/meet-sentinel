#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace meet_sentinel::google {

struct LoopbackCallback {
    std::string code;
    std::string state;
    std::string error;
    std::string raw_target;
};

class LoopbackAuthorizationServer {
public:
    explicit LoopbackAuthorizationServer(std::uint16_t preferred_port = 0, std::string callback_path = "/callback");
    ~LoopbackAuthorizationServer();

    LoopbackAuthorizationServer(const LoopbackAuthorizationServer&) = delete;
    LoopbackAuthorizationServer& operator=(const LoopbackAuthorizationServer&) = delete;

    std::uint16_t port() const;
    std::string redirect_uri() const;
    LoopbackCallback wait_for_callback(std::chrono::seconds timeout) const;

private:
    using SocketHandle = std::intptr_t;

    SocketHandle listen_socket_;
    std::uint16_t port_;
    std::string callback_path_;
};

} // namespace meet_sentinel::google

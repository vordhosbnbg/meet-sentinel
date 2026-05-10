#include "google/loopback_authorization_server.h"

#include <array>
#include <cerrno>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/select.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace meet_sentinel::google {
namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;
using SocketLength = int;

class SocketRuntime {
public:
    SocketRuntime() {
        WSADATA data{};
        if(WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error{"WSAStartup failed"};
        }
    }

    ~SocketRuntime() {
        WSACleanup();
    }
};

void ensure_socket_runtime() {
    static SocketRuntime runtime;
}

void close_socket(NativeSocket socket) {
    closesocket(socket);
}

int last_socket_error() {
    return WSAGetLastError();
}
#else
using NativeSocket = int;
constexpr NativeSocket kInvalidSocket = -1;
using SocketLength = socklen_t;

void ensure_socket_runtime() {}

void close_socket(NativeSocket socket) {
    close(socket);
}

int last_socket_error() {
    return errno;
}
#endif

class ScopedSocket {
public:
    explicit ScopedSocket(NativeSocket socket) : socket_{socket} {}

    ~ScopedSocket() {
        if(socket_ != kInvalidSocket) {
            close_socket(socket_);
        }
    }

    ScopedSocket(const ScopedSocket&) = delete;
    ScopedSocket& operator=(const ScopedSocket&) = delete;

    NativeSocket get() const {
        return socket_;
    }

private:
    NativeSocket socket_;
};

NativeSocket to_native(std::intptr_t socket) {
    return static_cast<NativeSocket>(socket);
}

std::intptr_t to_stored(NativeSocket socket) {
    return static_cast<std::intptr_t>(socket);
}

void throw_socket_error(std::string_view operation) {
    throw std::runtime_error{std::string{operation} + " failed with socket error " +
                             std::to_string(last_socket_error())};
}

std::uint16_t socket_port(NativeSocket socket) {
    sockaddr_in bound_address{};
    SocketLength size = sizeof(bound_address);
    if(getsockname(socket, reinterpret_cast<sockaddr*>(&bound_address), &size) != 0) {
        throw_socket_error("getsockname");
    }
    return ntohs(bound_address.sin_port);
}

NativeSocket create_listening_socket(std::uint16_t preferred_port) {
    ensure_socket_runtime();

    const NativeSocket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(socket == kInvalidSocket) {
        throw_socket_error("socket");
    }

    const int reuse_address = 1;
    setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse_address), sizeof(reuse_address));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(preferred_port);

    if(bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        const int error = last_socket_error();
        close_socket(socket);
        throw std::runtime_error{"bind failed with socket error " + std::to_string(error)};
    }

    if(listen(socket, 1) != 0) {
        const int error = last_socket_error();
        close_socket(socket);
        throw std::runtime_error{"listen failed with socket error " + std::to_string(error)};
    }

    return socket;
}

NativeSocket accept_with_timeout(NativeSocket listen_socket, std::chrono::seconds timeout) {
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(listen_socket, &read_set);

    timeval wait_time{
        .tv_sec = static_cast<long>(timeout.count()),
        .tv_usec = 0,
    };

    const int ready = select(static_cast<int>(listen_socket + 1), &read_set, nullptr, nullptr, &wait_time);
    if(ready == 0) {
        throw std::runtime_error{"timed out waiting for OAuth loopback callback"};
    }
    if(ready < 0) {
        throw_socket_error("select");
    }

    const NativeSocket client = accept(listen_socket, nullptr, nullptr);
    if(client == kInvalidSocket) {
        throw_socket_error("accept");
    }
    return client;
}

std::string read_request(NativeSocket client) {
    std::string request;
    std::array<char, 1024> buffer{};
    while(request.find("\r\n\r\n") == std::string::npos && request.size() < 16 * 1024) {
        const int received = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
        if(received <= 0) {
            break;
        }
        request.append(buffer.data(), static_cast<std::size_t>(received));
    }
    return request;
}

void send_response(NativeSocket client, std::string_view body) {
    const std::string response = "HTTP/1.1 200 OK\r\n"
                                 "Content-Type: text/html; charset=utf-8\r\n"
                                 "Connection: close\r\n"
                                 "Content-Length: " +
                                 std::to_string(body.size()) + "\r\n\r\n" + std::string{body};
    send(client, response.data(), static_cast<int>(response.size()), 0);
}

std::string request_target(std::string_view request) {
    const auto line_end = request.find("\r\n");
    const std::string_view request_line = request.substr(0, line_end);
    const auto first_space = request_line.find(' ');
    const auto second_space = request_line.find(' ', first_space + 1);
    if(first_space == std::string_view::npos || second_space == std::string_view::npos) {
        throw std::runtime_error{"invalid OAuth loopback HTTP request"};
    }

    return std::string{request_line.substr(first_space + 1, second_space - first_space - 1)};
}

int hex_value(char value) {
    if(value >= '0' && value <= '9') {
        return value - '0';
    }
    if(value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    if(value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    return -1;
}

std::string decode_query_value(std::string_view value) {
    std::string decoded;
    for(std::size_t index = 0; index < value.size(); ++index) {
        if(value[index] == '+') {
            decoded.push_back(' ');
            continue;
        }
        if(value[index] == '%' && index + 2 < value.size()) {
            const int high = hex_value(value[index + 1]);
            const int low = hex_value(value[index + 2]);
            if(high >= 0 && low >= 0) {
                decoded.push_back(static_cast<char>((high << 4) | low));
                index += 2;
                continue;
            }
        }
        decoded.push_back(value[index]);
    }
    return decoded;
}

std::vector<std::pair<std::string, std::string>> parse_query(std::string_view target) {
    const auto query_start = target.find('?');
    if(query_start == std::string_view::npos) {
        return {};
    }

    const auto fragment_start = target.find('#', query_start + 1);
    std::string_view query = target.substr(query_start + 1);
    if(fragment_start != std::string_view::npos) {
        query = target.substr(query_start + 1, fragment_start - query_start - 1);
    }

    std::vector<std::pair<std::string, std::string>> fields;
    while(!query.empty()) {
        const auto separator = query.find('&');
        const std::string_view field = query.substr(0, separator);
        const auto equals = field.find('=');
        if(equals != std::string_view::npos) {
            fields.push_back({
                decode_query_value(field.substr(0, equals)),
                decode_query_value(field.substr(equals + 1)),
            });
        }
        if(separator == std::string_view::npos) {
            break;
        }
        query.remove_prefix(separator + 1);
    }

    return fields;
}

LoopbackCallback callback_from_target(std::string target) {
    LoopbackCallback callback{
        .raw_target = std::move(target),
    };

    for(const auto& [name, value] : parse_query(callback.raw_target)) {
        if(name == "code") {
            callback.code = value;
        } else if(name == "state") {
            callback.state = value;
        } else if(name == "error") {
            callback.error = value;
        }
    }

    return callback;
}

} // namespace

LoopbackAuthorizationServer::LoopbackAuthorizationServer(std::uint16_t preferred_port, std::string callback_path) :
    listen_socket_{to_stored(create_listening_socket(preferred_port))}, port_{socket_port(to_native(listen_socket_))},
    callback_path_{std::move(callback_path)} {}

LoopbackAuthorizationServer::~LoopbackAuthorizationServer() {
    if(to_native(listen_socket_) != kInvalidSocket) {
        close_socket(to_native(listen_socket_));
    }
}

std::uint16_t LoopbackAuthorizationServer::port() const {
    return port_;
}

std::string LoopbackAuthorizationServer::redirect_uri() const {
    return "http://127.0.0.1:" + std::to_string(port_) + callback_path_;
}

LoopbackCallback LoopbackAuthorizationServer::wait_for_callback(std::chrono::seconds timeout) const {
    const ScopedSocket client{accept_with_timeout(to_native(listen_socket_), timeout)};

    const std::string request = read_request(client.get());
    const std::string target = request_target(request);
    send_response(client.get(), "Meet Sentinel authorization received. You can close this window.");

    return callback_from_target(target);
}

} // namespace meet_sentinel::google

#pragma once

#include <concepts>
#include <string>
#include <string_view>
#include <cstdint>


namespace wirekrak::core::transport::asio {

enum class ReceiveStatus {
    Ok,
    Closed,
    Timeout,
    RemoteClosed,
    ProtocolError,
    TransportError
};

template<class T>
concept ApiConcept = requires(
    T api,
    const T capi,
    std::string_view host,
    std::uint16_t port,
    std::string_view target,
    bool secure,
    std::string_view msg,
    void* buffer,
    std::size_t size,
    std::size_t& bytes
) {
    // Lifecycle
    { api.connect(host, port, target, secure) } -> std::same_as<bool>;
    { api.close() } -> std::same_as<void>;

    // State
    { capi.is_open() } -> std::same_as<bool>;

    // Data plane
    { api.send(msg) } -> std::same_as<bool>;

    // Streaming receive
    { api.read_some(buffer, size, bytes) } -> std::same_as<ReceiveStatus>;

    // Message boundary
    { capi.message_done() } -> std::same_as<bool>;
};

} // namespace wirekrak::core::transport::asio

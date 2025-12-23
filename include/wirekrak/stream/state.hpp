#pragma once

#include <cstdint>
#include <string_view>

#include "lcr/bit/pack.hpp"


namespace wirekrak::stream {

// ===============================================================
// CONNECTION STATE ENUM
// ===============================================================
enum class State : uint8_t {
    Connecting,
    Connected,
    Disconnecting,
    ForcedDisconnection,
    Disconnected,
    WaitingReconnect,
    Unknown
};

// ------------------------------------------------------------
// enum → string
// ------------------------------------------------------------
[[nodiscard]] inline constexpr std::string_view to_string(State s) noexcept {
    switch (s) {
        case State::Connecting:          return "connecting";
        case State::Connected:           return "connected";
        case State::Disconnecting:       return "disconnecting";
        case State::ForcedDisconnection: return "forced_disconnection";
        case State::Disconnected:        return "disconnected";
        case State::WaitingReconnect:    return "waiting_reconnect";
        default:                         return "unknown";
    }
}

} // namespace wirekrak::stream

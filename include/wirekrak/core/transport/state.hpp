#pragma once

#include <cstdint>
#include <string_view>

#include "lcr/bit/pack.hpp"


namespace wirekrak::core::transport {

// ===============================================================
// CONNECTION STATE ENUM
// ===============================================================
enum class State : uint8_t {
    Connecting,
    Connected,
    Disconnecting,
    WaitingReconnect,
    Disconnected,
    Unknown
};

// ------------------------------------------------------------
// State → string
// ------------------------------------------------------------
[[nodiscard]]
inline constexpr std::string_view to_string(State s) noexcept {
    switch (s) {
        case State::Connecting:          return "Connecting";
        case State::Connected:           return "Connected";
        case State::WaitingReconnect:    return "WaitingReconnect";
        case State::Disconnecting:       return "Disconnecting";
        case State::Disconnected:        return "Disconnected";
        default:                         return "Unknown";
    }
}


// ===============================================================
// EVENT ENUM
// ===============================================================
enum class Event : uint8_t {
    // --- User intent ---
    OpenRequested,
    CloseRequested,

    // --- Transport lifecycle --- 
    TransportConnected,   // Succeeded (open → connect)
    TransportConnectFailed,      // Failed (open → connect)
    TransportReconnectFailed,    // Failed (reconnect → connect)
    TransportClosed,

    // --- Liveness ---
    LivenessTimeout,

    // --- Retry ---
    RetryTimerExpired
};

// ===============================================================
// Event → string
// ===============================================================
[[nodiscard]]
inline constexpr std::string_view to_string(Event e) noexcept {
    switch (e) {
        case Event::OpenRequested:             return "OpenRequested";
        case Event::CloseRequested:            return "CloseRequested";
        case Event::TransportConnected:        return "TransportConnected";
        case Event::TransportConnectFailed:    return "TransportConnectFailed";
        case Event::TransportReconnectFailed:  return "TransportReconnectFailed";
        case Event::TransportClosed:           return "TransportClosed";
        case Event::LivenessTimeout:           return "LivenessTimeout";
        case Event::RetryTimerExpired:         return "RetryTimerExpired";
        default:                               return "UnknownEvent";
    }
}



// ===============================================================
// DISCONNECT REASON ENUM
// ===============================================================
enum class DisconnectReason : uint8_t {
    None,
    LocalClose,        // explicit close() by user
    TransportError,    // websocket / IO error
    LivenessTimeout    // heartbeat + message timeout
};

// ------------------------------------------------------------
// DisconnectReason → string
// ------------------------------------------------------------
[[nodiscard]]
inline constexpr std::string_view to_string(DisconnectReason r) noexcept {
    switch (r) {
        case DisconnectReason::None:            return "None";
        case DisconnectReason::LocalClose:      return "LocalClose";
        case DisconnectReason::TransportError:  return "TransportError";
        case DisconnectReason::LivenessTimeout: return "LivenessTimeout";
        default:                                return "Unknown";
    }
}

} // namespace wirekrak::core::transport

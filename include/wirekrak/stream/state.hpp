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
    Disconnected,
    WaitingReconnect,
    Unknown
};

// ------------------------------------------------------------
// enum → string
// ------------------------------------------------------------
[[nodiscard]] inline constexpr std::string_view to_string(State s) noexcept {
    switch (s) {
        case State::Connecting:       return "connecting";
        case State::Connected:        return "connected";
        case State::Disconnecting:    return "disconnecting";
        case State::Disconnected:     return "disconnected";
        case State::WaitingReconnect: return "waiting_reconnect";
        default:                          return "unknown";
    }
}

// ------------------------------------------------------------
// string → enum (safe slow path)
// ------------------------------------------------------------
[[nodiscard]] inline constexpr State to_conn_state(std::string_view s) noexcept {
    switch (s.size()) {
        case 10: // "connecting"
            if (s == "connecting") return State::Connecting;
            break;
        case 9: // "connected"
            if (s == "connected") return State::Connected;
            break;
        case 13: // "disconnecting"
            if (s == "disconnecting") return State::Disconnecting;
            break;
        case 12: // "disconnected"
            if (s == "disconnected") return State::Disconnected;
            break;
        case 17: // "waiting_reconnect"
            if (s == "waiting_reconnect") return State::WaitingReconnect;
            break;
    }
    return State::Unknown;
}

// ======================================================================
// FAST HASH VERSION USING pack4()
// ======================================================================

// Precomputed 4-byte packed tags
inline constexpr uint32_t TAG_CONN = lcr::bit::pack4("conn"); // connecting / connected
inline constexpr uint32_t TAG_DISC = lcr::bit::pack4("disc"); // disconnect*
inline constexpr uint32_t TAG_WAIT = lcr::bit::pack4("wait"); // waiting_reconnect

// Fast dispatcher
[[nodiscard]] inline constexpr State to_conn_state_fast(std::string_view s) noexcept {
    switch (lcr::bit::pack4(s)) {
        case TAG_CONN:
            // disambiguate by length
            return (s.size() == 9)  ? State::Connected
                 : (s.size() == 10) ? State::Connecting
                 : State::Unknown;

        case TAG_DISC:
            return (s.size() == 12) ? State::Disconnected
                 : (s.size() == 13) ? State::Disconnecting
                 : State::Unknown;

        case TAG_WAIT:
            return (s.size() == 17) ? State::WaitingReconnect
                                    : State::Unknown;

        default:
            return State::Unknown;
    }
}

} // namespace wirekrak::stream

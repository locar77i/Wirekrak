#pragma once

#include <cstdint>
#include <string_view>

#include "lcr/bit/pack.hpp"


namespace wirekrak::core::protocol::kraken {

// ===============================================
// SYSTEM STATE ENUM (status.system)
// ===============================================
enum class SystemState : uint8_t {
    Online,
    CancelOnly,
    Maintenance,
    PostOnly,
    Unknown
};

// -----------------------------------------------
// Convert enum → string
// -----------------------------------------------
[[nodiscard]] inline constexpr std::string_view to_string(SystemState s) noexcept {
    switch (s) {
        case SystemState::Online:       return "online";
        case SystemState::CancelOnly:   return "cancel_only";
        case SystemState::Maintenance:  return "maintenance";
        case SystemState::PostOnly:     return "post_only";
        default:                        return "unknown";
    }
}

// -----------------------------------------------
// Convert string → enum (safe, readable)
// -----------------------------------------------
[[nodiscard]] inline constexpr SystemState to_system_state_enum(std::string_view s) noexcept {
    switch (s.size()) {
        case 6: // "online"
            if (s[0] == 'o' && s == "online") return SystemState::Online;
            break;
        case 9: // "post_only"
            if (s[0] == 'p' && s == "post_only") return SystemState::PostOnly;
            break;
        case 11: // "cancel_only"
            if (s[0] == 'c' && s == "cancel_only") return SystemState::CancelOnly;
            break;
        case 11 + 1: // "maintenance" (12)
            if (s[0] == 'm' && s == "maintenance") return SystemState::Maintenance;
            break;
    }
    return SystemState::Unknown;
}

/*===============================================================
    FAST SYSTEM STATE PARSING
    - Uses first 4 bytes only
    - Matches Kraken WS v2 status.system values

        online        -> "onli"
        cancel_only  -> "canc"
        maintenance  -> "main"
        post_only    -> "post"
================================================================*/

// =========================
// Precomputed 4-byte tags
// =========================
inline constexpr uint32_t TAG_ONLI = lcr::bit::pack4("onli");
inline constexpr uint32_t TAG_CANC = lcr::bit::pack4("canc");
inline constexpr uint32_t TAG_MAIN = lcr::bit::pack4("main");
inline constexpr uint32_t TAG_POST = lcr::bit::pack4("post");

// -----------------------------------------------
// Fast string → enum
// -----------------------------------------------
[[nodiscard]] inline constexpr SystemState
to_system_state_enum_fast(std::string_view s) noexcept {
    switch (lcr::bit::pack4(s)) {
        case TAG_ONLI: return SystemState::Online;
        case TAG_CANC: return SystemState::CancelOnly;
        case TAG_MAIN: return SystemState::Maintenance;
        case TAG_POST: return SystemState::PostOnly;
        default:       return SystemState::Unknown;
    }
}

} // namespace wirekrak::core::protocol::kraken

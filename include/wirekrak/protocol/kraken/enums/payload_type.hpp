#pragma once

#include <cstdint>
#include <string_view>

#include "lcr/bit/pack.hpp"

namespace wirekrak::protocol::kraken {

// ===============================================
// PAYLOAD TYPE (snapshot / update)
// ===============================================
//
// Used by multiple Kraken channels (trade, book, etc.)
// Mirrors the "type" field in streaming messages.
//
enum class PayloadType : uint8_t {
    Snapshot,
    Update,
    Unknown
};

// ------------------------------------------------------------
// enum → string
// ------------------------------------------------------------
[[nodiscard]]
inline constexpr std::string_view to_string(PayloadType t) noexcept {
    switch (t) {
        case PayloadType::Snapshot: return "snapshot";
        case PayloadType::Update:   return "update";
        default:                    return "unknown";
    }
}

// ------------------------------------------------------------
// string → enum (safe path)
// ------------------------------------------------------------
[[nodiscard]]
inline constexpr PayloadType to_payload_type_enum(std::string_view s) noexcept {
    switch (s.size()) {
        case 6: // "update"
            if (s == "update") return PayloadType::Update;
            break;
        case 8: // "snapshot"
            if (s == "snapshot") return PayloadType::Snapshot;
            break;
    }
    return PayloadType::Unknown;
}

/*===============================================================
    FAST PAYLOAD TYPE PARSING
    - Uses 64-bit packed value
    - Zero branches except final dispatch
================================================================*/

// -------------------------
// Precomputed packed tags
// -------------------------
inline constexpr uint64_t TAG_UPDATE   = lcr::bit::pack8(std::string_view("update"));
inline constexpr uint64_t TAG_SNAPSHOT = lcr::bit::pack8(std::string_view("snapshot"));

// ------------------------------------------------------------
// string → enum (fast path)
// ------------------------------------------------------------
[[nodiscard]]
inline constexpr PayloadType to_payload_type_enum_fast(std::string_view s) noexcept {
    switch (lcr::bit::pack8(s)) {
        case TAG_UPDATE:   return PayloadType::Update;
        case TAG_SNAPSHOT: return PayloadType::Snapshot;
        default:           return PayloadType::Unknown;
    }
}

} // namespace wirekrak::protocol::kraken

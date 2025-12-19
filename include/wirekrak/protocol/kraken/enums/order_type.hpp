#pragma once

#include <cstdint>
#include <string_view>

#include "lcr/bit/pack.hpp"


namespace wirekrak::protocol::kraken {

// ===============================================================
// ORDER TYPE ENUM
// ===============================================================
enum class OrderType : uint8_t {
    Limit,
    Market,
    Unknown
};

// ------------------------------------------------------------
// enum → string
// ------------------------------------------------------------
[[nodiscard]] inline constexpr std::string_view to_string(OrderType t) noexcept {
    switch (t) {
        case OrderType::Limit:  return "limit";
        case OrderType::Market: return "market";
        default:                return "unknown";
    }
}

// ------------------------------------------------------------
// string → enum (safe slow path)
// ------------------------------------------------------------
[[nodiscard]] inline constexpr OrderType to_order_type_enum(std::string_view s) noexcept {
    switch (s.size()) {
        case 5: // "limit"
            if (s[0] == 'l' && s == "limit") return OrderType::Limit;
            break;
        case 6: // "market"
            if (s[0] == 'm' && s == "market") return OrderType::Market;
            break;
    }
    return OrderType::Unknown;
}

// ======================================================================
// FAST HASH VERSION USING pack4() — top-tier for high-frequency parsing
// ======================================================================

// Precomputed 4-byte packed tags
inline constexpr uint32_t TAG_LIMI = lcr::bit::pack4("limi");   // first 4 chars of "limit"
inline constexpr uint32_t TAG_MARK = lcr::bit::pack4("mark");   // first 4 chars of "market"

// Fast dispatcher
[[nodiscard]] inline constexpr OrderType to_order_type_enum_fast(std::string_view s) noexcept {
    switch (lcr::bit::pack4(s)) {
        case TAG_LIMI: return OrderType::Limit;
        case TAG_MARK: return OrderType::Market;
        default:       return OrderType::Unknown;
    }
}


} // namespace wirekrak::protocol::kraken

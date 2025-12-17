#pragma once

#include <cstdint>
#include <string_view>

#include "lcr/bit/pack.hpp"


namespace wirekrak {

// ===============================================
// TRADE SIDE ENUM
// ===============================================
enum class Side : uint8_t {
    Buy,
    Sell,
    Unknown
};
// Convert enum → string
[[nodiscard]] inline constexpr std::string_view to_string(Side s) noexcept {
    switch (s) {
        case Side::Buy:  return "buy";
        case Side::Sell: return "sell";
        default:         return "unknown";
    }
}
// Convert string → enum
[[nodiscard]] inline constexpr Side to_side_enum(std::string_view s) noexcept {
    switch (s.size()) {
        case 3: // "buy"
            if (s[0] == 'b' && s == "buy") return Side::Buy;
            break;
        case 4: // "sell"
            if (s[0] == 's' && s == "sell") return Side::Sell;
            break;
    }
    return Side::Unknown;
}
/*===============================================================
    FAST SIDE PARSING (buy / sell)
    - Uses 32-bit packed value
    - Zero branches except final dispatch
================================================================*/
// =========================
// Precomputed 4-byte tags
// =========================
inline constexpr uint32_t TAG_BUY  = lcr::bit::pack4("buy");   // padded as: 'b','u','y',0
inline constexpr uint32_t TAG_SELL = lcr::bit::pack4("sell");  // 4 chars

inline constexpr Side to_side_enum_fast(std::string_view s) noexcept {
    switch (lcr::bit::pack4(s)) {
        case TAG_BUY:  return Side::Buy;
        case TAG_SELL: return Side::Sell;
        default:   return Side::Unknown;
    }
}


} // namespace wirekrak

#pragma once

#include <cstdint>
#include <string_view>

#include "lcr/bit/pack.hpp"


namespace wirekrak::core::protocol::kraken {

// ===============================================
// CHANNEL ENUM
// ===============================================
enum class Channel : uint8_t {
    Trade,
    Ticker,
    Book,
    Heartbeat,
    Status,
    Unknown
};

// Convert enum → string
[[nodiscard]] inline constexpr std::string_view to_string(Channel c) noexcept {
    switch (c) {
        case Channel::Trade:     return "trade";
        case Channel::Ticker:    return "ticker";
        case Channel::Book:      return "book";
        case Channel::Heartbeat: return "heartbeat";
        case Channel::Status:    return "status";
        default:                 return "unknown";
    }
}

// Convert string → enum
[[nodiscard]] inline constexpr Channel to_channel_enum(std::string_view s) noexcept{
    switch (s.size()) {
        case 4: // "book"
            if (s[0] == 'b' && s == "book") return Channel::Book;
            break;
        case 5: // "trade"
            if (s[0] == 't' && s == "trade") return Channel::Trade;
            break;
        case 6: // "ticker", "status"
            if (s[0] == 't' && s == "ticker") return Channel::Ticker;
            if (s[0] == 's' && s == "status") return Channel::Status;
            break;
        case 9: // "heartbeat"
            if (s[0] == 'h' && s == "heartbeat") return Channel::Heartbeat;
            break;
    }
    return Channel::Unknown;
}
/*===============================================================
    FAST CHANNEL PARSING (trade, ticker, book, heartbeat, status)
    - Uses 4-byte fast dispatch
    - Words >4 chars use first 4 bytes only:
        ticker    -> "tick"
        heartbeat -> "hear"
        status    -> "stat"
================================================================*/

// =========================
// Precomputed 4-byte tags
// =========================
inline constexpr uint32_t TAG_TRADE = lcr::bit::pack4("trad");
inline constexpr uint32_t TAG_TICK  = lcr::bit::pack4("tick");
inline constexpr uint32_t TAG_BOOK  = lcr::bit::pack4("book");
inline constexpr uint32_t TAG_HEAR  = lcr::bit::pack4("hear");
inline constexpr uint32_t TAG_STAT  = lcr::bit::pack4("stat");

inline constexpr Channel to_channel_enum_fast(std::string_view s) noexcept {
    switch (lcr::bit::pack4(s)) {
        case TAG_TRADE: return Channel::Trade;
        case TAG_TICK:  return Channel::Ticker;
        case TAG_BOOK:  return Channel::Book;
        case TAG_HEAR:  return Channel::Heartbeat;
        case TAG_STAT:  return Channel::Status;
        default:    return Channel::Unknown;
    }
}

} // namespace wirekrak::core::protocol::kraken

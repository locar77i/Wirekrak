#pragma once

#include <string_view>

#include "lcr/bit/pack.hpp"


namespace wirekrak {
    template <typename>
    inline constexpr bool always_false = false;
}

namespace wirekrak {

// ===============================================================
// METHOD ENUM
// ===============================================================
enum class Method : uint8_t {
    Subscribe,
    Unsubscribe,
    Ping,
    Pong,
    Unknown
};

// ===============================================================
// 1) Standard conversion: enum → string
// ===============================================================
[[nodiscard]] inline constexpr std::string_view to_string(Method m) noexcept {
    switch (m) {
        case Method::Subscribe:   return "subscribe";
        case Method::Unsubscribe: return "unsubscribe";
        case Method::Ping:        return "ping";
        case Method::Pong:        return "pong";
        default:                  return "unknown";
    }
}

// ===============================================================
// 2) Standard conversion: string → enum
//    (fallback version: readable, slower)
// ===============================================================
[[nodiscard]] inline constexpr Method to_method_enum(std::string_view s) noexcept {
    switch (s.size()) {
        case 4: // ping, pong
            if (s[1] == 'i' && s == "ping") return Method::Ping;
            if (s[1] == 'o' && s == "pong") return Method::Pong;
            break;
        case 9: // subscribe
            if (s[0] == 's' && s == "subscribe") return Method::Subscribe;
            break;
        case 11: // unsubscribe
            if (s[0] == 'u' && s == "unsubscribe") return Method::Unsubscribe;
            break;
    }
    return Method::Unknown;
}

// ===============================================================
// 3) FAST lookups using 4-byte hashing
// ===============================================================
// These pack ONLY the first four characters of the method.
// For fast dispatch we only need to discriminate based on prefixes.
// Kraken method names are unique by their first 4 chars.

// "sub*" (subscribe)
inline constexpr uint32_t TAG_SUBS = lcr::bit::pack4("subs");
inline constexpr uint32_t TAG_UNSU = lcr::bit::pack4("unsu");
inline constexpr uint32_t TAG_PING = lcr::bit::pack4("ping");
inline constexpr uint32_t TAG_PONG = lcr::bit::pack4("pong");

[[nodiscard]] inline constexpr Method to_method_enum_fast(std::string_view s) noexcept {
    switch (lcr::bit::pack4(s)) {
        case TAG_SUBS: return Method::Subscribe;
        case TAG_UNSU: return Method::Unsubscribe;
        case TAG_PING: return Method::Ping;
        case TAG_PONG: return Method::Pong;
        default:       return Method::Unknown;
    }
}


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


} // namespace wirekrak

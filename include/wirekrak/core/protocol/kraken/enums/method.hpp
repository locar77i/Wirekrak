#pragma once

#include <cstdint>
#include <string_view>

#include "lcr/bit/pack.hpp"


namespace wirekrak::core::protocol::kraken {

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

} // namespace wirekrak::core::protocol::kraken

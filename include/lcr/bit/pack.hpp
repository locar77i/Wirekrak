#pragma once
#include <string_view>
#include <cstdint>

namespace lcr {
namespace bit {

// Packs up to 4 chars into a uint32_t (little endian)
constexpr uint32_t pack4(const char* s) noexcept {
    return
        (uint32_t(uint8_t(s[0]))) |
        (uint32_t(uint8_t(s[1])) << 8) |
        (uint32_t(uint8_t(s[2])) << 16) |
        (uint32_t(uint8_t(s[3])) << 24);
}

// Packs string_view (pads with zeros)
constexpr uint32_t pack4(std::string_view s) noexcept {
    return pack4(s.data());
}


// ============================================================================
// pack8
// ============================================================================

// Packs up to 8 chars into a uint64_t (little endian)
constexpr uint64_t pack8(const char* s) noexcept {
    return
        (uint64_t(uint8_t(s[0]))) |
        (uint64_t(uint8_t(s[1])) << 8)  |
        (uint64_t(uint8_t(s[2])) << 16) |
        (uint64_t(uint8_t(s[3])) << 24) |
        (uint64_t(uint8_t(s[4])) << 32) |
        (uint64_t(uint8_t(s[5])) << 40) |
        (uint64_t(uint8_t(s[6])) << 48) |
        (uint64_t(uint8_t(s[7])) << 56);
}

// Packs string_view (pads with zeros)
constexpr uint64_t pack8(std::string_view s) noexcept {
    const char* p = s.data();
    return
        (s.size() > 0 ? uint64_t(uint8_t(p[0]))       : 0) |
        (s.size() > 1 ? uint64_t(uint8_t(p[1])) << 8  : 0) |
        (s.size() > 2 ? uint64_t(uint8_t(p[2])) << 16 : 0) |
        (s.size() > 3 ? uint64_t(uint8_t(p[3])) << 24 : 0) |
        (s.size() > 4 ? uint64_t(uint8_t(p[4])) << 32 : 0) |
        (s.size() > 5 ? uint64_t(uint8_t(p[5])) << 40 : 0) |
        (s.size() > 6 ? uint64_t(uint8_t(p[6])) << 48 : 0) |
        (s.size() > 7 ? uint64_t(uint8_t(p[7])) << 56 : 0);
}

} // namespace bit
} // namespace lcr

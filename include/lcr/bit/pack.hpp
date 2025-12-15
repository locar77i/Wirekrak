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

} // namespace bit
} // namespace lcr

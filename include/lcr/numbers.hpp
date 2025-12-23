#pragma once

#include <cstdint>
#include <type_traits>


namespace lcr {

// -----------------------------------------------------------------------------
// Branchless round up to next power of two for 32-bit
[[nodiscard]] inline std::uint32_t round_up_to_power_of_two_32(std::uint32_t n) noexcept {
    if (n <= 1) return 1;
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return ++n;
}

// -----------------------------------------------------------------------------
// Branchless round up to next power of two for 64-bit
[[nodiscard]] inline std::uint64_t round_up_to_power_of_two_64(std::uint64_t n) noexcept {
    if (n <= 1) return 1;
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return ++n;
}

} // namespace lcr

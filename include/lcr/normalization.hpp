#pragma once

#include <cstdint>
#include <cassert>
#include <cmath>

#include "lcr/trap.hpp"


namespace lcr {

// Normalize floating tick_size into integer units with bounded precision
[[nodiscard]] inline constexpr std::uint64_t normalize_tick_size(double tick_units, std::int64_t &out_scaled_tick, int max_pow10 = 9) noexcept
{
    LCR_ASSERT_MSG(tick_units > 0.0, "tick_units must be > 0");
    long double t = tick_units;
    std::uint64_t mult = 1;
    // Try increasing powers of ten until we get a non-zero integer tick
    for (int i = 0; i <= max_pow10; ++i) {
        long double scaled = t * static_cast<long double>(mult);
        long long rounded = std::llround(scaled);
        if (rounded > 0) {
            out_scaled_tick = static_cast<std::int64_t>(rounded);
            return mult;
        }
        mult *= 10;
    }
    // fallback: 1 tick at scale 1
    out_scaled_tick = 1;

    LCR_UNREACHABLE(); // tick_units has too many decimal places, fallback to 1

    return 1;
}

} // namespace lcr

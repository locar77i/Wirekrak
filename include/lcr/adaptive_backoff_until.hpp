#pragma once

#include <thread>
#include <chrono>
#include <cstddef>
#include <utility>
#include <immintrin.h> // _mm_pause

namespace lcr {

/**
 * Adaptive backoff loop.
 *
 * Template parameters:
 *   Op:  () -> bool       operation attempted repeatedly until success
 *   Stop: () -> bool      external stop predicate (e.g., shutdown flag)
 *
 * Returns:
 *   true  → operation succeeded
 *   false → stop condition activated before success
 */
 /*
template <typename Op, typename Stop>
inline bool adaptive_backoff_until(Op&& op, Stop&& stop, size_t spin1 = 2000, size_t spin2 = 10000, std::chrono::microseconds sleep_time = std::chrono::microseconds(50)) noexcept {
    size_t spins = 0;
    while (true) {
        // Try the operation
        if (op()) [[likely]] {
            return true;
        }
        // Stop condition (e.g., shutdown requested)
        if (stop()) [[unlikely]] {
            return false;
        }
        // Adaptive backoff logic
        if (spins < spin1) {
            lcr::system::cpu_relax();
        } else if (spins < spin2) {
            std::this_thread::yield();
        } else {
            std::this_thread::sleep_for(sleep_time);
        }
        ++spins;
    }
}
*/
template <typename Op, typename Stop>
inline bool adaptive_backoff_until(
    Op&& op,
    Stop&& stop,
    size_t spin1 = 50000,      // Stage 1: pure CPU spin
    size_t spin2 = 150000,     // Stage 2: light scheduler hint
    std::chrono::nanoseconds sleep_time = std::chrono::nanoseconds(0)
) noexcept
{
    size_t spins = 0;

    while (true) {
        // 1. Try the operation
        if (op()) [[likely]]
            return true;
        // 2. Stop condition
        if (stop()) [[unlikely]]
            return false;
        // 3. Adaptive backoff
        if (spins < spin1) { // Tight spinning (best for SPSC rings)
            _mm_pause();
        }
        else if (spins < spin2) { // Light yield: cross-platform, macro-free, safe for Windows
            std::this_thread::sleep_for(std::chrono::nanoseconds(0));
        }
        else { // Very light fallback sleep
            std::this_thread::sleep_for(sleep_time);
        }

        ++spins;
    }
}

} // namespace lcr

#pragma once

#include <cstdint>

#include "lcr/system/monotonic_clock.hpp"


namespace lcr::metrics::util {

// ============================================================================
// scope_timer
// ============================================================================
//
// RAII helper for measuring execution duration of a scope.
//
// The timer captures a timestamp on construction and records the elapsed
// duration when the object goes out of scope (destructor).
//
// Requirements on Metric:
//   metric.record(start_ns, end_ns)
//
// Typical usage:
//
//     WK_TL3(
//         lcr::metrics::util::scope_timer timer{telemetry_.poll_duration};
//     );
//
// The destructor guarantees recording even if the scope exits via:
//
//   - return
//   - break
//   - exception
//   - early exit
//
// Designed for ultra-low-latency instrumentation:
//   - zero allocations
//   - no locks
//   - only two clock reads
// ============================================================================

template <typename DurationMetric>
class scope_timer {
public:

    using clock_type = lcr::system::monotonic_clock;

    explicit scope_timer(DurationMetric& metric) noexcept
        : metric_(metric),
          start_ns_(clock_type::instance().now_ns())
    {}

    ~scope_timer() noexcept {
        const auto end_ns = clock_type::instance().now_ns();
        metric_.record(start_ns_, end_ns);
    }

    scope_timer(const scope_timer&) = delete;
    scope_timer& operator=(const scope_timer&) = delete;

private:

    DurationMetric& metric_;
    std::uint64_t   start_ns_;
};

} // namespace lcr::metrics::util

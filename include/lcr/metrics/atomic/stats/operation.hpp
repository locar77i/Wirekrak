#pragma once

#include <string>
#include <sstream>
#include <atomic>
#include <type_traits>
#include <cstdint>
#include <limits>

#include "lcr/metrics/atomic/counter.hpp"
#include "lcr/metrics/atomic/gauge.hpp"
#include "lcr/system/cpu_relax.hpp"
#include "lcr/format.hpp"


namespace lcr {
namespace metrics {
namespace atomic {
namespace stats {

// ---------------------------------------------------------------------------
// operation
// ---------------------------------------------------------------------------
// Pattern for tracking counts and durations of high-frequency operations.
//
// Fields:
//   total_ns_    — cumulative total latency in nanoseconds
//   samples_     — total attempts (e.g. enforced, triggered, executed)
//   success_     — successful operations
//   min_ns_      — smallest latency observed
//   max_ns_      — largest latency observed
//
// Semantics:
//   • all atomics use relaxed memory ordering
//   • no locks, minimal cache line contention
//   • record() and record_success() callable from hot path
// ---------------------------------------------------------------------------
template <typename T = uint64_t>
struct alignas(64) operation {
    // Constructor
    operation() = default;
    // Disable copy/move semantics
    operation(const operation&) = delete;
    operation& operator=(const operation&) = delete;
    operation(operation&&) noexcept = delete;
    operation& operator=(operation&&) noexcept = delete;

    // Specialized copy method
    void copy_to(operation& other) const noexcept {
        other.total_ns_.store(total_ns_.load());
        other.samples_.store(samples_.load());
        other.success_.store(success_.load());
        other.min_ns_.store(min_ns_.load());
        other.max_ns_.store(max_ns_.load());
    }

    inline void record(T start_ns, T end_ns, bool ok = true) noexcept {
        record_duration(end_ns - start_ns, ok);
    }

    inline void record_duration(T delta, bool ok = true) noexcept {
        total_ns_.inc(delta);
        samples_.inc();
        if (ok) success_.inc();
        // Update min
        T prev = min_ns_.load();
        while (delta < prev && !min_ns_.compare_exchange_weak(prev, delta))
            lcr::system::cpu_relax();
        // Update max
        prev = max_ns_.load();
        while (delta > prev && !max_ns_.compare_exchange_weak(prev, delta))
            lcr::system::cpu_relax();
    }

    // Accessors
    inline std::uint64_t samples() const noexcept {
        return samples_.load();
    }

    inline std::uint64_t total_ns() const noexcept {
        return total_ns_.load();
    }

    inline T min_ns() const noexcept {
        return min_ns_.load();
    }

    inline T max_ns() const noexcept {
        return max_ns_.load();
    }

    // Derived metrics
    inline uint64_t avg_ns() const noexcept {
        const auto count = samples_.load();
        if (count == 0) return 0;
        return total_ns_.load() / count;
    }

    inline uint64_t jitter_ns() const noexcept {
        const auto count = samples_.load();
        if (count < 2) return 0;
        return max_ns_.load() - min_ns_.load();
    }

    inline double rate_per_sec() const noexcept {
        const auto n = samples_.load();
        const auto t_ns = total_ns_.load();
        if (n == 0 || t_ns == 0) return 0.0;
        return (static_cast<double>(n) * 1'000'000'000.0) / static_cast<double>(t_ns);
    }

    inline T failures() const noexcept {
        return samples_.load() - success_.load();
    }

    inline double success_rate() const noexcept {
        const auto n = samples_.load();
        return n ? static_cast<double>(success_.load()) / n : 0.0;
    }

    // -----------------------------------------------------------------------
    // Reset (careful — only call from single-threaded context)
    // -----------------------------------------------------------------------
    inline void reset() noexcept {
        total_ns_.reset();
        samples_.reset();
        success_.reset();
        min_ns_.store(std::numeric_limits<T>::max());
        max_ns_.reset();
    }

    inline void dump(std::ostream& os) const noexcept {
        T samples = samples_.load();
        if (samples == 0) {
            os << "samples=0";
        }
        else if (samples >= 1) {
            os << "samples=" << samples << " [ok=" << success_.load() << " fail=" << failures() << "]";
            os << " total=" << lcr::format_duration(total_ns_.load());
        }
        if (samples >= 2) {
            os << " min=" << lcr::format_duration(min_ns_.load())
                << " max=" << lcr::format_duration(max_ns_.load())
                << " avg=" << lcr::format_duration(avg_ns())
                << " rate=" << lcr::format_throughput(rate_per_sec());
        }
    }

    // Optional string formatter (for debug or Prometheus output)
    inline std::string str() const noexcept {
        std::ostringstream os;
        dump(os);
        return os.str();
    }

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& prefix, Collector& collector) const noexcept {
        T samples = samples_.load();
        samples_.collect(prefix + "_samples_total", "Number of recorded samples", collector);
        success_.collect(prefix + "_success_total", "Number of successful operations recorded", collector);
        if (samples >= 1) {
            total_ns_.collect(prefix + "_total_ns", "Total duration in nanoseconds", collector);
        }
        if (samples >= 2) {
            min_ns_.collect(prefix + "_min_ns", "Minimum observed duration in nanoseconds", collector);
            max_ns_.collect(prefix + "_max_ns", "Maximum observed duration in nanoseconds", collector);
            collector.add_gauge(avg_ns(), prefix + "_avg_ns", "Average duration in nanoseconds");
            collector.add_gauge(jitter_ns(), prefix + "_jitter_ns", "Absolute jitter (max - min) in nanoseconds");
            collector.add_gauge(rate_per_sec(), prefix + "_rate_per_second", "Rate of observed samples per second");
        }
    }

private:
    counter64 total_ns_{};
    counter64 samples_{};
    counter64 success_{};
    gauge<T>  min_ns_{std::numeric_limits<T>::max()};
    gauge<T>  max_ns_{};
};

// Fixed-width specialization for hot path
using operation64 = operation<uint64_t>;
static_assert(std::is_standard_layout_v<operation64>, "operation64 must be standard layout");

} // namespace stats
} // namespace atomic
} // namespace metrics
} // namespace lcr

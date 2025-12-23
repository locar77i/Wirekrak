#pragma once

#include <string>
#include <sstream>
#include <atomic>
#include <type_traits>
#include <cstdint>

#include "lcr/metrics/counter.hpp"
#include "lcr/metrics/gauge.hpp"
#include "lcr/system/cpu_relax.hpp"
#include "lcr/time_unit.hpp"
#include "lcr/format.hpp"


namespace lcr {
namespace metrics {
namespace stats {

// ---------------------------------------------------------------------------
// duration
// ---------------------------------------------------------------------------
// Tracks latency/duration statistics.
// - samples:   number of samples recorded
// - total_ns:  total accumulated time
// - min_ns:    minimum observed duration
// - max_ns:    maximum observed duration
//
// Template parameter T = integer type for counters (default: uint64_t)
//
// No multithreading guarantees — use only from single thread or via snapshot copies.
// ---------------------------------------------------------------------------
template <typename T = uint64_t>
struct alignas(64) duration {
    // Constructor
    duration() = default;
    // Disable copy/move semantics
    duration(const duration&) = delete;
    duration& operator=(const duration&) = delete;
    duration(duration&&) noexcept = delete;
    duration& operator=(duration&&) noexcept = delete;

    // Specialized copy method
    void copy_to(duration& other) const noexcept {
        other.total_ns_.store(total_ns_.load());
        other.samples_.store(samples_.load());
        other.min_ns_.store(min_ns_.load());
        other.max_ns_.store(max_ns_.load());
    }

    inline void record(T start_ns, T end_ns) noexcept {
        record_duration(end_ns - start_ns);
    }

    inline void record_duration(T delta) noexcept {
        total_ns_.inc(delta);
        samples_.inc();
        if (delta < min_ns_.load()) min_ns_.store(delta);
        if (delta > max_ns_.load()) max_ns_.store(delta);
    }

    // Raw metrics with unit conversion
    inline double total(time_unit unit = time_unit::nanoseconds) const noexcept {
        return convert_ns(static_cast<double>(total_ns_.load()), unit);
    }

    inline double min(time_unit unit = time_unit::nanoseconds) const noexcept {
        return convert_ns(static_cast<double>(min_ns_.load()), unit);
    }

    inline double max(time_unit unit = time_unit::nanoseconds) const noexcept {
        return convert_ns(static_cast<double>(max_ns_.load()), unit);
    }

    // Derived metrics
    inline double avg(time_unit unit = time_unit::nanoseconds) const noexcept {
        const auto n = samples_.load();
        if (n == 0) return 0.0;
        return convert_ns(total_ns_.load() / static_cast<double>(n), unit);
    }

    inline double jitter(time_unit unit = time_unit::nanoseconds) const noexcept {
       const auto count = samples_.load();
        if (count < 2) return 0.0;
        return convert_ns(static_cast<double>(max_ns_.load() - min_ns_.load()), unit);
    }

    inline double rate_per_sec() const noexcept {
        const auto n = samples_.load();
        const auto t_ns = total_ns_.load();
        if (n == 0 || t_ns == 0) return 0.0;
        return (static_cast<double>(n) * 1'000'000'000.0) / static_cast<double>(t_ns);
    }

    // Reset (careful — only call from single-threaded context)
    inline void reset() noexcept {
        total_ns_.reset();
        samples_.reset();
        min_ns_.store(std::numeric_limits<T>::max());
        max_ns_.reset();
    }

    // Optional string formatter (for debug or Prometheus output)
    inline std::string str(time_unit tunit = time_unit::seconds, time_unit unit = time_unit::milliseconds) const {
        std::ostringstream oss;
        T samples = samples_.load();
        oss << "samples=" << samples;
        if (samples >= 1) {
            oss << " total=" << total(tunit) << to_string(tunit);
        }
        if (samples >= 2) {
            oss << " min=" << min(unit) << to_string(unit)
                << " max=" << max(unit) << to_string(unit)
                << " avg=" << avg(unit) << to_string(unit)
                << " rate=" << lcr::format_throughput(rate_per_sec());
        }
        return oss.str();
    }

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& prefix, Collector& collector) const noexcept {
        T samples = samples_.load();
        samples_.collect(prefix + "_samples_total", "Number of recorded samples", collector);
        if (samples >= 1) {
            total_ns_.collect(prefix + "_total_ns", "Total duration in nanoseconds", collector);
        }
        if (samples >= 2) {
            min_ns_.collect(prefix + "_min_ns", "Minimum observed duration in nanoseconds", collector);
            max_ns_.collect(prefix + "_max_ns", "Maximum observed duration in nanoseconds", collector);
            collector.add_gauge(avg(), prefix + "_avg_ns", "Average duration in nanoseconds");
            collector.add_gauge(jitter(), prefix + "_jitter_ns", "Absolute jitter (max - min) in nanoseconds");
            collector.add_gauge(rate_per_sec(), prefix + "_rate_per_second", "Rate of observed samples per second");
        }
    }

private:
    counter<T> total_ns_{};
    counter<T> samples_{};
    gauge<T>   min_ns_{std::numeric_limits<T>::max()};
    gauge<T>   max_ns_{};
};

// Fixed-width specialization for hot path
using duration64 = duration<uint64_t>;
static_assert(std::is_standard_layout_v<duration64>, "duration64 must be standard layout");

} // namespace stats
} // namespace metrics
} // namespace lcr

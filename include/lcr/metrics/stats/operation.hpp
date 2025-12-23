#pragma once

#include <string>
#include <sstream>
#include <atomic>
#include <type_traits>
#include <cstdint>

#include "lcr/metrics/counter.hpp"
#include "lcr/metrics/gauge.hpp"
#include "lcr/time_unit.hpp"
#include "lcr/system/cpu_relax.hpp"
#include "lcr/format.hpp"


namespace lcr {
namespace metrics {
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
// No multithreading guarantees — use only from single thread or via snapshot copies.
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
        if (delta < min_ns_.load()) min_ns_.store(delta);
        if (delta > max_ns_.load()) max_ns_.store(delta);
    }

    // Accessors for raw counts
    inline T samples() const noexcept {
        return samples_.load();
    }
    inline T total_ns() const noexcept {
        return total_ns_.load();
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
        const auto count = samples_.load();
        if (count == 0) return 0.0;
        return convert_ns(static_cast<double>(total_ns_.load()) / count, unit);
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

    // Optional string formatter (for debug or Prometheus output)
    inline std::string str(time_unit tunit = time_unit::seconds, time_unit unit = time_unit::milliseconds) const {
        std::ostringstream oss;
        T samples = samples_.load();
        if (samples == 0) {
            oss << "samples=0";
        }
        else if (samples >= 1) {
            oss << "samples=" << samples << " [ok=" << success_.load() << " fail=" << failures() << "]";
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
        success_.collect(prefix + "_success_total", "Number of successful operations recorded", collector);
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
    counter<T> success_{};
    gauge<T>   min_ns_{std::numeric_limits<T>::max()};
    gauge<T>   max_ns_{};
};

// Fixed-width specialization for hot path
using operation64 = operation<uint64_t>;
static_assert(std::is_standard_layout_v<operation64>, "operation64 must be standard layout");

} // namespace stats
} // namespace metrics
} // namespace lcr

#pragma once

#include <string>
#include <sstream>
#include <atomic>
#include <type_traits>
#include <cstdint>

#include "lcr/metrics/atomic/counter.hpp"
#include "lcr/metrics/atomic/gauge.hpp"
#include "lcr/system/cpu_relax.hpp"
#include "lcr/format.hpp"


namespace lcr {
namespace metrics {
namespace atomic {
namespace stats {

// ---------------------------------------------------------------------------
// size
// ---------------------------------------------------------------------------
// Tracks simple size-related statistics atomically, lock-free.
// Useful for metrics like:
//  - active orders
//  - partitions used
//  - buffer fill level
//  - outstanding messages
//
// Template parameter T = integer counter type (default uint64_t)
//
// Provides atomic min/max tracking, average, and instantaneous value.
// All atomics use relaxed memory ordering for minimal cache contention.
// ---------------------------------------------------------------------------
template <typename T = uint64_t>
struct alignas(64) size {
    // Constructor
    size() = default;
    // Disable copy/move semantics
    size(const size&) = delete;
    size& operator=(const size&) = delete;
    size(size&&) noexcept = delete;
    size& operator=(size&&) noexcept = delete;

    // Specialized copy method
    void copy_to(size& other) const noexcept {
        other.last_.store(last_.load());
        other.accumulated_.store(accumulated_.load());
        other.samples_.store(samples_.load());
        other.min_.store(min_.load());
        other.max_.store(max_.load());
    }

    // Increment/decrement last count
    inline void inc(T delta = 1) noexcept {
        const T new_val = last_.add(delta);
        update_extremes_(new_val);
    }

    inline void dec(T delta = 1) noexcept {
        const T new_val = last_.sub(delta);
        update_extremes_(new_val);
    }

    // Set explicitly (useful when value is recomputed externally)
    inline void set(T value) noexcept {
        last_.store(value);
        update_extremes_(value);
    }

    // Derived metrics
    inline double avg() const noexcept {
        const auto n = samples_.load();
        if (n == 0) return 0.0;
        return static_cast<double>(accumulated_.load()) / static_cast<double>(n);
    }

    // Reset (safe only in single-threaded context)
    inline void reset() noexcept {
        last_.reset();
        accumulated_.reset();
        samples_.reset();
        min_.store(std::numeric_limits<T>::max());
        max_.reset();
    }

    // String formatter (for human-readable or Prometheus output)
    inline std::string str() const {
        std::ostringstream oss;
        oss << " samples=" << lcr::format_number_exact(samples_.load())
            << " last=" << lcr::format_number_exact(last_.load())
            << " min=" << lcr::format_number_exact(min_.load())
            << " max=" << lcr::format_number_exact(max_.load())
            << " avg=" << avg();
        return oss.str();
    }

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& prefix, Collector& collector) const noexcept {
        collector.add_gauge(last_.load(), prefix + "_last", "Last observed value");
        collector.add_gauge(avg(), prefix + "_avg", "Average observed value");
        collector.add_gauge(min_.load(), prefix + "_min", "Minimum observed value");
        collector.add_gauge(max_.load(), prefix + "_max", "Maximum observed value");
        collector.add_counter(samples_.load(), prefix + "_samples_total", "Number of samples recorded");
    }

private:
    gauge<T>    last_{};
    counter<T>  accumulated_{};
    counter<T>  samples_{};
    gauge<T>    min_{std::numeric_limits<T>::max()};
    gauge<T>    max_{};
    // Private methods/helpers --------------------------------------------------------------

    inline void update_extremes_(T value) noexcept {
        accumulated_.add(value);
        samples_.inc();

        T prev = min_.load();
        while (value < prev && !min_.compare_exchange_weak(prev, value))
            lcr::system::cpu_relax();

        prev = max_.load();
        while (value > prev && !max_.compare_exchange_weak(prev, value))
            lcr::system::cpu_relax();
    }
};

// Fixed width specialization for hot path
using size32 = size<uint32_t>;
static_assert(std::is_standard_layout_v<size32>, "size32 must be standard layout");
using size64 = size<uint64_t>;
static_assert(std::is_standard_layout_v<size64>, "size64 must be standard layout");

} // namespace stats
} // namespace atomic
} // namespace metrics
} // namespace lcr

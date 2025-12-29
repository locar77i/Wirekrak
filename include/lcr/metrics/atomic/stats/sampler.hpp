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
// sampler
// ---------------------------------------------------------------------------
// Records a stream of observed values ("samples") and maintains basic
// statistical aggregates:
//
//   - linear probe lengths
//   - hash collisions
//   - retry attempts
//   - micro-latency measurements
//
// Tracks:
//   • sample count
//   • total (sum)
//   • minimum and maximum observed values
//   • average
//   • rate of samples per second
//
// All fields are updated using atomic operations.
// The sampler is non-copyable and intended for concurrent use.
// ---------------------------------------------------------------------------
template <typename T = uint64_t>
struct alignas(64) sampler {
    // Constructor
    sampler() = default;
    // Disable copy/move semantics
    sampler(const sampler&) = delete;
    sampler& operator=(const sampler&) = delete;
    sampler(sampler&&) noexcept = delete;
    sampler& operator=(sampler&&) noexcept = delete;

    // Specialized copy method
    void copy_to(sampler& other) const noexcept {
        other.total_.store(total_.load());
        other.samples_.store(samples_.load());
        other.min_.store(min_.load());
        other.max_.store(max_.load());
    }

    inline void record(T value) noexcept {
        total_.inc(value);
        samples_.inc();

        T prev = min_.load();
        while (value < prev && !min_.compare_exchange_weak(prev, value))
            lcr::system::cpu_relax();

        prev = max_.load();
        while (value > prev && !max_.compare_exchange_weak(prev, value))
            lcr::system::cpu_relax();
    }

    // Derived metrics
    inline double avg() const noexcept {
        const auto n = samples_.load();
        if (n == 0) return 0.0;
        return static_cast<double>(total_.load()) / static_cast<double>(n);
    }

    // Reset (careful — only call from single-threaded context)
    inline void reset() noexcept {
        total_.reset();
        samples_.reset();
        min_.store(std::numeric_limits<T>::max());
        max_.reset();
    }

    inline std::string str() const {
        std::ostringstream oss;
        T samples = samples_.load();
        oss << "samples=" << lcr::format_number_exact(samples);
        if (samples >= 1) {
            oss << " total=" << lcr::format_number_exact(total_.load());
        }
        if (samples >= 2) {
            oss << " min=" << lcr::format_number_exact(min_.load())
                << " max=" << lcr::format_number_exact(max_.load())
                << " avg=" << avg();
        }
        return oss.str();
    }

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& prefix, Collector& collector) const noexcept {
        T samples = samples_.load();
        samples_.collect(prefix + "_samples_total", "Number of samples observed", collector);
        if (samples >= 1) {
            total_.collect(prefix + "_total", "Total of all observed values (sum)", collector);
        }
        if (samples >= 2) {
            min_.collect(prefix + "_min", "Minimum observed value", collector);
            max_.collect(prefix + "_max", "Maximum observed value", collector);
            collector.add_gauge(avg(), prefix + "_avg", "Average observed value");
        }
    }

private:
    counter<T>  total_{};
    counter<T>  samples_{};
    gauge<T>    min_{std::numeric_limits<T>::max()};
    gauge<T>    max_{};
};
using sampler32 = sampler<uint32_t>;
static_assert(std::is_standard_layout_v<sampler32>, "sampler32 must be standard layout");
using sampler64 = sampler<uint64_t>;
static_assert(std::is_standard_layout_v<sampler64>, "sampler64 must be standard layout");

} // namespace stats
} // namespace atomic
} // namespace metrics
} // namespace lcr

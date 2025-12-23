#pragma once

#include <string>
#include <array>
#include <sstream>
#include <atomic>
#include <numeric>
#include <type_traits>
#include <cstdint>

#include "lcr/time_unit.hpp"


namespace lcr {
namespace metrics {
namespace atomic {

// ---------------------------------------------------------------------------
// latency_percentiles
// ---------------------------------------------------------------------------
// Simple struct to hold latency percentiles
// ---------------------------------------------------------------------------
struct latency_percentiles {
    uint64_t p50{0};
    uint64_t p90{0};
    uint64_t p99{0};
    uint64_t p999{0};
    uint64_t p9999{0};
    uint64_t p99999{0};
    uint64_t p999999{0};
    std::string str(time_unit unit = time_unit::nanoseconds) const noexcept {
        std::ostringstream oss;
        oss << "Latency Percentiles: "
            << " p50=" << convert_ns(p50, unit) << to_string(unit)
            << " p90=" << convert_ns(p90, unit) << to_string(unit)
            << " p99=" << convert_ns(p99, unit) << to_string(unit)
            << " p99.9=" << convert_ns(p999, unit) << to_string(unit)
            << " p99.99=" << convert_ns(p9999, unit) << to_string(unit)
            << " p99.999=" << convert_ns(p99999, unit) << to_string(unit)
            << " p99.9999=" << convert_ns(p999999, unit) << to_string(unit);
        return oss.str();
    }
};

// ---------------------------------------------------------------------------
// latency_histogram
// ---------------------------------------------------------------------------
// Lock-free logarithmic histogram for percentile estimation.
// Each record() updates a single bucket via relaxed atomic increment.
// Percentiles are computed offline (not thread-safe).
// ---------------------------------------------------------------------------
// This implementation is correct for single-threaded use, but not safe for multi-threaded writes — false sharing can occur between buckets.
struct alignas(64) latency_histogram {
    // Constructor
    latency_histogram() noexcept { reset(); }
    // Disable copy/move semantics
    latency_histogram(const latency_histogram&) = delete;
    latency_histogram& operator=(const latency_histogram&) = delete;
    latency_histogram(latency_histogram&&) noexcept = delete;
    latency_histogram& operator=(latency_histogram&&) noexcept = delete;

    // Specialized copy method
    void copy_to(latency_histogram& other) const noexcept {
        for (int i = 0; i < kNumBuckets_; ++i) {
            other.buckets_[i].store(buckets_[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
    }

    inline void record(uint64_t start_ns, uint64_t end_ns) noexcept {
        uint64_t delta = end_ns - start_ns;
        int bucket = delta ? std::min<int>(63, 63 - __builtin_clzll(delta)) : 0;
        buckets_[bucket].fetch_add(1, std::memory_order_relaxed);
    }

    latency_percentiles compute_percentiles() const noexcept {
        latency_percentiles result;
        std::array<uint64_t, kNumBuckets_> local{};
        for (int i = 0; i < kNumBuckets_; ++i)
            local[i] = buckets_[i].load(std::memory_order_relaxed);

        uint64_t total = std::accumulate(local.begin(), local.end(), 0ULL);
        if (total == 0) return {};

        const uint64_t p50_t  = total / 2;
        const uint64_t p90_t  = total * 90 / 100;
        const uint64_t p99_t  = total * 99 / 100;
        const uint64_t p999_t = total * 999 / 1000;
        const uint64_t p9999_t = total * 9999 / 10000;
        const uint64_t p99999_t = total * 99999 / 100000;
        const uint64_t p999999_t = total * 999999 / 1000000;

        uint64_t cumulative = 0;

        for (int i = 0; i < kNumBuckets_; ++i) {
            cumulative += local[i];
            if (cumulative >= p50_t  && result.p50 == 0)  result.p50  = (1ULL << i);
            if (cumulative >= p90_t  && result.p90 == 0)  result.p90  = (1ULL << i);
            if (cumulative >= p99_t  && result.p99 == 0)  result.p99  = (1ULL << i);
            if (cumulative >= p999_t && result.p999 == 0) result.p999 = (1ULL << i);
            if (cumulative >= p9999_t && result.p9999 == 0) result.p9999 = (1ULL << i);
            if (cumulative >= p99999_t && result.p99999 == 0) result.p99999 = (1ULL << i);
            if (cumulative >= p999999_t && result.p999999 == 0) result.p999999 = (1ULL << i);
        }

        return result;
    }

    inline void reset() noexcept {
        for (auto& b : buckets_)
            b.store(0, std::memory_order_relaxed);
    }

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& name, Collector& collector) const noexcept {
        const auto percentiles = compute_percentiles();
        collector.add_summary(percentiles, name, "Latency percentiles");
        // Jitter metrics
        uint64_t p99_jitter = percentiles.p99 - percentiles.p50;
        collector.add_gauge(static_cast<double>(p99_jitter), name + "_p99_jitter_ns", "Main latency jitter between p50 and p99 in nanoseconds");
        uint64_t p999_jitter = percentiles.p999 - percentiles.p50;
        collector.add_gauge(static_cast<double>(p999_jitter), name + "_p999_jitter_ns", "Tail latency jitter between p50 and p999 in nanoseconds");
        uint64_t p9999_jitter = percentiles.p9999 - percentiles.p50;
        collector.add_gauge(static_cast<double>(p9999_jitter), name + "_p9999_jitter_ns", "Ultra-tail latency jitter between p50 and p9999 in nanoseconds (very high jitter)");
        uint64_t p99999_jitter = percentiles.p99999 - percentiles.p50;
        collector.add_gauge(static_cast<double>(p99999_jitter), name + "_p99999_jitter_ns", "Extreme-tail latency jitter between p50 and p99999 in nanoseconds (extreme jitter)");
        uint64_t p999999_jitter = percentiles.p999999 - percentiles.p50;
        collector.add_gauge(static_cast<double>(p999999_jitter), name + "_p999999_jitter_ns", "Ultra-extreme-tail latency jitter between p50 and p999999 in nanoseconds (ultra extreme jitter)");
    }

private:
    static constexpr int kNumBuckets_ = 64;
    std::atomic<uint64_t> buckets_[kNumBuckets_];
};

} // namespace atomic
} // namespace metrics
} // namespace lcr

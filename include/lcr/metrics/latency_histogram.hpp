#pragma once

#include <string>
#include <array>
#include <sstream>
#include <numeric>
#include <type_traits>
#include <cstdint>

#include "lcr/format.hpp"


namespace lcr {
namespace metrics {

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

    inline void dump(std::ostream& os) const noexcept {
        os << "Latency Percentiles: "
            << " p50=" << lcr::format_duration(p50)
            << " p90=" << lcr::format_duration(p90)
            << " p99=" << lcr::format_duration(p99)
            << " p99.9=" << lcr::format_duration(p999)
            << " p99.99=" << lcr::format_duration(p9999)
            << " p99.999=" << lcr::format_duration(p99999)
            << " p99.9999=" << lcr::format_duration(p999999);
    }

    inline std::string str() const noexcept {
        std::ostringstream os;
        dump(os);
        return os.str();
    }
};

// ---------------------------------------------------------------------------
// latency_histogram
// ---------------------------------------------------------------------------
//
// No multithreading guarantees — use only from single thread or via snapshot copies.
// ---------------------------------------------------------------------------
struct alignas(64) latency_histogram {
    // Constructor
    latency_histogram() noexcept { reset(); }
    // Disable copy/move semantics
    latency_histogram(const latency_histogram&) = delete;
    latency_histogram& operator=(const latency_histogram&) = delete;
    latency_histogram(latency_histogram&&) = delete;
    latency_histogram& operator=(latency_histogram&&) = delete;

    // Specialized copy method
    inline void copy_to(latency_histogram& dst) const noexcept {
        for (int i = 0; i < kNumBuckets_; ++i)
            dst.buckets_[i] = buckets_[i];
    }

    // record() – main hot-path method (extremely fast)
    inline void record(uint64_t start_ns, uint64_t end_ns) noexcept {
        uint64_t delta = end_ns - start_ns;
        // bucket = floor(log2(delta))
        int bucket = (delta == 0) ? 0 : (63 - __builtin_clzll(delta));
        buckets_[bucket] += 1;
    }

    // Computes latency percentiles offline
    latency_percentiles compute_percentiles() const noexcept {
        latency_percentiles result{};
        std::array<uint64_t, kNumBuckets_> local{};
        // local copy for stable iteration
        for (int i = 0; i < kNumBuckets_; ++i)
            local[i] = buckets_[i];

        const uint64_t total = std::accumulate(local.begin(), local.end(), 0ULL);
        if (total == 0) return result;

        const uint64_t p50_t     = total / 2;
        const uint64_t p90_t     = total * 90 / 100;
        const uint64_t p99_t     = total * 99 / 100;
        const uint64_t p999_t    = total * 999 / 1000;
        const uint64_t p9999_t   = total * 9999 / 10000;
        const uint64_t p99999_t  = total * 99999 / 100000;
        const uint64_t p999999_t = total * 999999 / 1000000;

        uint64_t cumulative = 0;

        auto update = [&](uint64_t& field, uint64_t threshold, int i) {
            if (field == 0 && cumulative >= threshold)
                field = (1ULL << i);
        };

        for (int i = 0; i < kNumBuckets_; ++i) {
            cumulative += local[i];
            update(result.p50,     p50_t,     i);
            update(result.p90,     p90_t,     i);
            update(result.p99,     p99_t,     i);
            update(result.p999,    p999_t,    i);
            update(result.p9999,   p9999_t,   i);
            update(result.p99999,  p99999_t,  i);
            update(result.p999999, p999999_t, i);
        }

        return result;
    }

    // Reset
    inline void reset() noexcept {
        for (auto& b : buckets_)
            b = 0;
    }

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& name, Collector& collector) const noexcept {
        const auto pct = compute_percentiles();
        collector.add_summary(pct, name, "Latency percentiles");
        // Jitter metrics
        auto jitter = [&](uint64_t hi) {
            return hi - pct.p50;
        };
        collector.add_gauge(static_cast<double>(jitter(pct.p99)), name + "_p99_jitter_ns", "Main latency jitter between p50 and p99 in nanoseconds");
        collector.add_gauge(static_cast<double>(jitter(pct.p999)), name + "_p999_jitter_ns", "Tail latency jitter between p50 and p999 in nanoseconds");
        collector.add_gauge(static_cast<double>(jitter(pct.p9999)), name + "_p9999_jitter_ns", "Ultra-tail latency jitter between p50 and p9999 in nanoseconds (very high jitter)");
        collector.add_gauge(static_cast<double>(jitter(pct.p99999)), name + "_p99999_jitter_ns", "Extreme-tail latency jitter between p50 and p99999 in nanoseconds (extreme jitter)");
        collector.add_gauge(static_cast<double>(jitter(pct.p999999)), name + "_p999999_jitter_ns", "Ultra-extreme-tail latency jitter between p50 and p999999 in nanoseconds (ultra extreme jitter)");
    }

private:
    static constexpr int kNumBuckets_ = 64;
    uint64_t buckets_[kNumBuckets_]; // POD buckets (fastest possible)
};

} // namespace metrics
} // namespace lcr

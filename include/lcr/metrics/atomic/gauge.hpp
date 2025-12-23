#pragma once

#include <string>
#include <atomic>
#include <type_traits>
#include <cstdint>


namespace lcr {
namespace metrics {
namespace atomic {

// ---------------------------------------------------------------------------
// gauge - A metric that can go up and down (Instantaneous state)
// ---------------------------------------------------------------------------
template<typename T = int64_t>
struct alignas(64) gauge {
    // Constructor
    gauge() = default;
    explicit gauge(T initial) noexcept : value_(initial) {}

    // Disable copy/move semantics
    gauge(const gauge&) = delete;
    gauge& operator=(const gauge&) = delete;
    gauge(gauge&&) noexcept = delete;
    gauge& operator=(gauge&&) noexcept = delete;

    // Specialized copy method
    void copy_to(gauge& other) const noexcept {
        other.value_.store(value_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    // Accessor
    inline T load() const noexcept { return value_.load(std::memory_order_relaxed); }
    // Mutators
    inline void store(T v) noexcept { value_.store(v, std::memory_order_relaxed); }
    inline void inc(T n = 1) noexcept { value_.fetch_add(n, std::memory_order_relaxed); }
    inline void dec(T n = 1) noexcept { value_.fetch_sub(n, std::memory_order_relaxed); }
    // Return-after-update mutators
    inline T add(T n) noexcept { return value_.fetch_add(n, std::memory_order_relaxed) + n; }
    inline T sub(T n) noexcept { return value_.fetch_sub(n, std::memory_order_relaxed) - n; }
    // CAS wrapper (same semantics as std::atomic<T>). Needed for min/max updating in AtomicSizeStats
    inline bool compare_exchange_weak(T& expected, T desired) noexcept {
        return value_.compare_exchange_weak(
            expected,
            desired,
            std::memory_order_relaxed,
            std::memory_order_relaxed
        );
    }
    // Reset gauge to zero
    inline void reset() noexcept { value_.store(0, std::memory_order_relaxed); }

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& name, const std::string& help, Collector& collector) const noexcept {
        collector.add_gauge(load(), name, help);
    }

private:
    std::atomic<T> value_{0};
};
// Fixed-width specialization for hot path
using gauge32 = gauge<uint32_t>;
static_assert(std::is_standard_layout_v<gauge32>, "gauge32 must be standard layout");
using gauge64 = gauge<uint64_t>;
static_assert(std::is_standard_layout_v<gauge64>, "gauge64 must be standard layout");

} // namespace atomic
} // namespace metrics
} // namespace lcr

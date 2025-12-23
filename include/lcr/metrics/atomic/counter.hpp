#pragma once

#include <string>
#include <atomic>
#include <type_traits>
#include <cstdint>


namespace lcr {
namespace metrics {
namespace atomic {

// ---------------------------------------------------------------------------
// counter - A simple monotonically increasing counter (Cumulative metric)
// ---------------------------------------------------------------------------
template<typename T = uint64_t>
struct alignas(64) counter {
    // Constructor
    counter() = default;
    explicit counter(T initial) noexcept : value_(initial) {}
    // Disable copy/move semantics
    counter(const counter&) = delete;
    counter& operator=(const counter&) = delete;
    counter(counter&&) noexcept = delete;
    counter& operator=(counter&&) noexcept = delete;

    // Specialized copy method
    void copy_to(counter& other) const noexcept {
        other.value_.store(value_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    // Accessor
    inline T load() const noexcept { return value_.load(std::memory_order_relaxed); }
    // Mutators
    inline void store(T v) noexcept { value_.store(v, std::memory_order_relaxed); }
    inline void inc(T n = 1) noexcept { value_.fetch_add(n, std::memory_order_relaxed); }
    // Return-after-update mutator
    inline T add(T n) noexcept { return value_.fetch_add(n, std::memory_order_relaxed) + n; }

    // Reset counter to zero
    inline void reset() noexcept { value_.store(0, std::memory_order_relaxed); }

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& name, const std::string& help, Collector& collector) const noexcept {
        collector.add_counter(load(), name, help);
    }

private:
    std::atomic<T> value_{0};
};
// Fixed-width specialization for hot path
using counter32 = counter<uint32_t>;
static_assert(std::is_standard_layout_v<counter32>, "counter32 must be standard layout");
using counter64 = counter<uint64_t>;
static_assert(std::is_standard_layout_v<counter64>, "counter64 must be standard layout");

} // namespace atomic
} // namespace metrics
} // namespace lcr

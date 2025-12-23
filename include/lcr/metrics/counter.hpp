#pragma once

#include <string>
#include <type_traits>
#include <cstdint>

namespace lcr {
namespace metrics {

// ---------------------------------------------------------------------------
// counter - A simple monotonically increasing counter (Cumulative metric)
// ---------------------------------------------------------------------------
//
// No multithreading guarantees — use only from single thread or via snapshot copies.
// ---------------------------------------------------------------------------
template<typename T = uint64_t>
struct alignas(64) counter {
public:
    // Constructor
    constexpr counter() noexcept = default;
    constexpr explicit counter(T initial) noexcept : value_(initial) {}
    // Disable copy/move semantics
    counter(const counter&) = delete;
    counter& operator=(const counter&) = delete;
    counter(counter&&) noexcept = delete;
    counter& operator=(counter&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(counter& dst) const noexcept {
        dst.value_ = value_;   // trivial assignment, zero overhead
    }

    // Accessor
    inline constexpr T load() const noexcept {  return value_; }
    // Mutators
    inline constexpr void store(T v) noexcept { value_ = v; }
    inline constexpr void inc(T n = 1) noexcept { value_ += n;}
    // Return-after-update mutator
    inline constexpr T add(T n) noexcept {
        value_ += n;
        return value_;
    }

    // Reset counter to zero
    inline constexpr void reset() noexcept { value_ = 0; }

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& name, const std::string& help, Collector& collector) const noexcept {
        collector.add_counter(load(), name, help);
    }

private:
    T value_{0};   // Plain POD value (no atomics needed)
};
// Fixed-width specialization for hot path
using counter32 = counter<uint32_t>;
static_assert(std::is_standard_layout_v<counter32>, "counter32 must be standard layout");
using counter64 = counter<uint64_t>;
static_assert(std::is_standard_layout_v<counter64>, "counter64 must be standard layout");

} // namespace metrics
} // namespace lcr

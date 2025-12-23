#pragma once

#include <string>
#include <type_traits>
#include <cstdint>

namespace lcr {
namespace metrics {

// ---------------------------------------------------------------------------
// gauge - A metric that can go up and down (Instantaneous state)
// ---------------------------------------------------------------------------
//
// No multithreading guarantees — use only from single thread or via snapshot copies.
// ---------------------------------------------------------------------------
template<typename T = int64_t>
struct alignas(64) gauge {
public:
    // Constructor
    constexpr gauge() noexcept = default;
    constexpr explicit gauge(T initial) noexcept : value_(initial) {}

    // Disable copy/move semantics
    gauge(const gauge&) = delete;
    gauge& operator=(const gauge&) = delete;
    gauge(gauge&&) noexcept = delete;
    gauge& operator=(gauge&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(gauge& other) const noexcept {
        other.value_ = value_;
    }

    // Accessor
    inline constexpr T load() const noexcept { return value_; }
    // Mutators
    inline constexpr void store(T v) noexcept { value_ = v; }
    inline constexpr void inc(T n = 1) noexcept { value_ += n; }
    inline constexpr void dec(T n = 1) noexcept { value_ -= n; }
    // Return-after-update mutators
    inline constexpr T add(T n) noexcept {
        value_ += n;
        return value_;
    }
    inline constexpr T sub(T n) noexcept {
        value_ -= n;
        return value_;
    }

    // Reset gauge to zero
    inline constexpr void reset() noexcept { value_ = 0; }

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& name, const std::string& help, Collector& collector) const noexcept {
        collector.add_gauge(value_, name, help);
    }

private:
    T value_{0};   // POD, no atomics needed
};
// Fixed-width specialization for hot path
using gauge32 = gauge<uint32_t>;
static_assert(std::is_standard_layout_v<gauge32>, "gauge32 must be standard layout");
using gauge64 = gauge<uint64_t>;
static_assert(std::is_standard_layout_v<gauge64>, "gauge64 must be standard layout");

} // namespace metrics
} // namespace lcr

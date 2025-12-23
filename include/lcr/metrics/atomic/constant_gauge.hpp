#pragma once

#include <string>
#include <type_traits>
#include <cstdint>


namespace lcr {
namespace metrics {
namespace atomic {

// ---------------------------------------------------------------------------
// constant_gauge: represents a fixed metric value (e.g. configuration constants)
// It doesn't need to be atomic since it is immutable after initialization.
// ---------------------------------------------------------------------------
template<typename T = uint64_t>
struct alignas(64) constant_gauge {
    // Constructors
    constexpr constant_gauge() noexcept = default;
    constexpr explicit constant_gauge(T value) noexcept : value_(value) {}

    // Disable copy/move semantics (consistent with other metrics)
    constant_gauge(const constant_gauge&) = delete;
    constant_gauge& operator=(const constant_gauge&) = delete;
    constant_gauge(constant_gauge&&) noexcept = delete;
    constant_gauge& operator=(constant_gauge&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(constant_gauge& other) const noexcept {
        other.value_ = value_;
    }

    // Accessors
    [[nodiscard]] inline constexpr T load() const noexcept { return value_; }
    [[nodiscard]] inline constexpr operator T() const noexcept { return value_; }

    // Initialization helper (intended for startup/configuration only)
    inline void set(T value) noexcept { value_ = value; }

    // Reset is a no-op for constants (kept for API symmetry)
    inline void reset() noexcept {}

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& name, const std::string& help, Collector& collector) const noexcept {
        collector.add_gauge(load(), name, help);
    }

private:
    T value_{0};
};
// Fixed-width specialization for hot path
using constant_gauge_u32 = constant_gauge<uint32_t>;
static_assert(std::is_standard_layout_v<constant_gauge_u32>, "constant_gauge_u32 must be standard layout");
using constant_gauge_u64 = constant_gauge<uint64_t>;
static_assert(std::is_standard_layout_v<constant_gauge_u64>, "constant_gauge_u64 must be standard layout");

} // namespace atomic
} // namespace metrics
} // namespace lcr

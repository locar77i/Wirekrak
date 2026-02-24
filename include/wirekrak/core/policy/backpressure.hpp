#pragma once

#include <concepts>

namespace wirekrak::core::policy {

// ============================================================================
// Backpressure Mode
// ============================================================================
//
// Transport detects saturation.
// Policy only classifies behavior timing.
// Transport executes mechanics.
// Session owns strategy.
//
// ZeroTolerance -> signal immediately and force close
// Strict        -> signal immediately and let session decide fate
// Relaxed       -> tolerate temporarily before signal to let session decide fate
// ============================================================================

enum class BackpressureMode {
    ZeroTolerance,
    Strict,
    Relaxed
};

// ============================================================================
// Backpressure Policy Concept
// ============================================================================
//
// A BackpressurePolicy must expose:
//
//   static constexpr BackpressureMode on_ring_full() noexcept;
//
// Policy must:
//   - Be pure
//   - Be side-effect free
//   - Not access state
//   - Be constexpr evaluable
// ============================================================================

template<typename P>
concept BackpressurePolicy =
requires {
    { P::on_ring_full() } noexcept -> std::same_as<BackpressureMode>;
};

// ============================================================================
// Backpressure Policy Implementations
// ============================================================================

namespace backpressure {

// Immediate signaling
struct ZeroTolerance {
    static constexpr BackpressureMode on_ring_full() noexcept {
        return BackpressureMode::ZeroTolerance;
    }
};

// Immediate signaling
struct Strict {
    static constexpr BackpressureMode on_ring_full() noexcept {
        return BackpressureMode::Strict;
    }
};

// Delayed signaling (tolerates temporary saturation)
struct Relaxed {
    static constexpr BackpressureMode on_ring_full() noexcept {
        return BackpressureMode::Relaxed;
    }
};

} // namespace backpressure

} // namespace wirekrak::core::policy
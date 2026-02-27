#pragma once

#include <chrono>
#include <concepts>

namespace wirekrak::core::policy::transport {

/*
===============================================================================
 Transport Liveness Policy
===============================================================================

This policy defines how transport-level liveness is evaluated.

Liveness is a *transport concern* and measures observable traffic activity
(rx or tx progress). If no traffic is observed within a configured time
window, the connection is considered stale.

The policy is:

- Compile-time defined
- Zero runtime polymorphism
- Zero dynamic configuration
- Deterministic per Connection type

-------------------------------------------------------------------------------
 Design Principles
-------------------------------------------------------------------------------

• Liveness is about transport activity, not protocol semantics.
• Liveness is evaluated only while Connected.
• Warning and expiration are edge-triggered.
• Policy defines thresholds, not behavior.
• Connection executes mechanics.

-------------------------------------------------------------------------------
 Modes
-------------------------------------------------------------------------------

1) Disabled
   - No liveness checks
   - No warnings
   - No forced reconnects

2) Enabled<Timeout, WarningPercent>
   - Liveness timeout after Timeout milliseconds of inactivity
   - Warning emitted when (Timeout * WarningPercent / 100) is reached

-------------------------------------------------------------------------------
 Example
-------------------------------------------------------------------------------

using MyTransportPolicies = transport::connection_bundle<
    backpressure::Strict<>,
    liveness::Enabled<std::chrono::seconds(15), 0.8>
>;

===============================================================================
*/


// ============================================================================
// Liveness Policy Concept
// ============================================================================
//
// A valid LivenessPolicy must expose:
//
//   static constexpr bool enabled;
//   static constexpr std::chrono::milliseconds timeout;
//   static constexpr double warning_ratio;
//
// If enabled == false, timeout and warning_ratio are ignored.
//
// ============================================================================

template<typename P>
concept LivenessPolicy =
requires {
    { P::enabled } -> std::same_as<const bool&>;
    { P::timeout } -> std::convertible_to<std::chrono::milliseconds>;
    { P::warning_percent } -> std::convertible_to<std::uint32_t>;
};


namespace liveness {

// ============================================================================
// Disabled Liveness
// ============================================================================
//
// No liveness evaluation.
// Connection will never emit liveness warnings or expirations.
//
// ============================================================================

struct Disabled {

    static constexpr bool enabled = false;

    // Unused placeholders (required for concept satisfaction)
    static constexpr std::chrono::milliseconds timeout{0};
    static constexpr std::uint32_t warning_percent{0};
};


// ============================================================================
// Enabled Liveness
// ============================================================================
//
// Enables deterministic transport liveness monitoring.
//
// Template Parameters:
//   Timeout       -> total silence window
//   WarningPercent  -> fraction of Timeout before warning (0 < r < 100)
//
// Example:
//   Enabled<15000, 80> -> Warning at 12s, expiration at 15s
//
// Semantics:
//   - Warning emitted once when remaining time <= (Timeout * WarningPercent / 100)
//   - Expiration emitted once when silence > Timeout
//
// ============================================================================

template<
    std::uint32_t TimeoutMs = 15000,   // 15 seconds
    std::uint32_t WarningPercent = 80  // 80%
>
requires (TimeoutMs > 0) &&
         (WarningPercent > 0) &&
         (WarningPercent < 100)
struct Enabled {

    static constexpr bool enabled = true;

    static constexpr std::chrono::milliseconds timeout{TimeoutMs};

    static constexpr std::uint32_t warning_percent = WarningPercent;
};

} // namespace liveness

} // namespace wirekrak::core::policy::transport

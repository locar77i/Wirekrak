#pragma once

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

#include <chrono>
#include <concepts>

namespace wirekrak::core::policy::transport {


// ============================================================================
// Liveness Policy Concept
// ============================================================================
//
// A valid LivenessConcept defines how transport-level inactivity is evaluated.
//
// Liveness is strictly a transport concern. It measures observable traffic
// activity (rx or tx progress) and determines when a connection should be
// considered stale.
//
// This concept enforces both:
//
//   1) Structural correctness  (required static members exist)
//   2) Semantic correctness    (values are valid when enabled)
//
// -----------------------------------------------------------------------------
// Required Static Members
// -----------------------------------------------------------------------------
//
//   static constexpr bool enabled;
//   static constexpr std::chrono::milliseconds timeout;
//   static constexpr std::uint32_t warning_percent;
//
// Semantics:
//
//   • If enabled == false:
//       - timeout and warning_percent are ignored
//       - No liveness evaluation occurs
//
//   • If enabled == true:
//       - timeout.count() > 0
//       - 0 < warning_percent < 100
//
// All checks are performed at compile time.
// No runtime overhead.
// No dynamic configuration.
// No inheritance required.
//
// -----------------------------------------------------------------------------

template<typename P>
concept HasLivenessMembers =
requires {
    // Structural requirements only
    { P::enabled } -> std::same_as<const bool&>;
    { P::timeout } -> std::convertible_to<std::chrono::milliseconds>;
    { P::warning_percent } -> std::same_as<const std::uint32_t&>;
};

template<typename P>
concept LivenessConcept =
    HasLivenessMembers<P>
    &&
    (
        // Disabled mode
        (
            !P::enabled
        )
        ||
        // Enabled mode (semantic validation)
        (
            P::enabled &&
            (P::timeout.count() > 0) &&
            (P::warning_percent > 0) &&
            (P::warning_percent < 100)
        )
    );


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

// Assert that Disabled satisfies the LivenessConcept
static_assert(LivenessConcept<Disabled>, "liveness::Disabled does not satisfy LivenessConcept");


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

// Assert that Enabled satisfies the LivenessConcept
static_assert(LivenessConcept<Enabled<>>, "liveness::Enabled does not satisfy LivenessConcept");

} // namespace liveness


// ============================================================================
// Default Liveness Policy
// ============================================================================

using DefaultLiveness = liveness::Enabled<>; // Alias for Enabled with default parameters

// Assert that DefaultLiveness satisfies the LivenessConcept
static_assert(LivenessConcept<DefaultLiveness>, "DefaultLiveness does not satisfy LivenessConcept");

} // namespace wirekrak::core::policy::transport

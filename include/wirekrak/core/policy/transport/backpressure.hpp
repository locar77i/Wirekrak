#pragma once

#include <concepts>
#include <ostream>

#include "wirekrak/core/config/backpressure.hpp"
#include "wirekrak/core/policy/backpressure_mode.hpp"
#include "lcr/control/binary_hysteresis.hpp"


namespace wirekrak::core::policy::transport {

// ============================================================================
// Backpressure Policy Concept
// ============================================================================
//
// A valid BackpressureConcept must define:
//
//   static constexpr BackpressureMode mode;
//   using hysteresis;
//
// Semantics:
//
//   • If mode == ZeroTolerance:
//         hysteresis must be std::nullptr_t
//
//   • If mode == Strict or Relaxed:
//         hysteresis must NOT be std::nullptr_t
//
// All validation is compile-time.
// Zero runtime overhead.
//
// ============================================================================

template<typename P>
concept HasBackpressureMembers =
requires {
    { P::mode } -> std::same_as<const BackpressureMode&>;
    typename P::hysteresis;
};

template<typename P>
concept BackpressureConcept =
    HasBackpressureMembers<P>
    &&
    (
        // ZeroTolerance → no hysteresis allowed
        (
            P::mode == BackpressureMode::ZeroTolerance &&
            std::same_as<typename P::hysteresis, std::nullptr_t>
        )
        ||
        // Strict / Relaxed → hysteresis required
        (
            (P::mode == BackpressureMode::Strict ||
             P::mode == BackpressureMode::Relaxed)
            &&
            (!std::same_as<typename P::hysteresis, std::nullptr_t>)
        )
    );

// ============================================================================
// Backpressure Policy Implementations
// ============================================================================

namespace backpressure {

// ------------------------------------------------------------
// ZeroTolerance
// ------------------------------------------------------------
// Immediate activation
// No recovery (transport forces close on backpressure)

struct ZeroTolerance {

    static constexpr BackpressureMode mode = BackpressureMode::ZeroTolerance;

    using hysteresis = std::nullptr_t;

    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // ------------------------------------------------------------

    static constexpr const char* mode_name() noexcept {
        return "ZeroTolerance";
    }

    static void dump(std::ostream& os) {
        os << "[Transport Backpressure Policy]\n";
        os << "- Mode        : " << mode_name() << "\n";
        os << "- Hysteresis  : none\n";
        os << "- Behavior    : Immediate close on first saturation\n\n";
    }
};

// Assert that ZeroTolerance satisfies the BackpressureConcept
static_assert(BackpressureConcept<ZeroTolerance>, "backpressure::ZeroTolerance does not satisfy BackpressureConcept");


// ------------------------------------------------------------
// Strict
// ------------------------------------------------------------
// Immediate activation
// Stabilized recovery

template<
    std::uint32_t DeactivateThreshold = config::backpressure::STRICT_HYSTERESIS_DEACTIVATION_THRESHOLD
>
requires (DeactivateThreshold > 0)
struct Strict {

    static constexpr BackpressureMode mode = BackpressureMode::Strict;

    using hysteresis = lcr::control::BinaryHysteresis<config::backpressure::STRICT_HYSTERESIS_ACTIVATION_THRESHOLD, DeactivateThreshold>;

    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // ------------------------------------------------------------

    static constexpr const char* mode_name() noexcept {
        return "Strict";
    }

    static void dump(std::ostream& os) {
        os << "[Transport Backpressure Policy]\n";
        os << "- Mode        : " << mode_name() << "\n";
        os << "- Hysteresis  : enabled\n";
        os << "- Activation  : " << config::backpressure::STRICT_HYSTERESIS_ACTIVATION_THRESHOLD << " (threshold)\n";
        os << "- Deactivation: " << DeactivateThreshold << " (threshold)\n";
        os << "- Behavior    : Immediate activation, stabilized recovery\n\n";
    }
};

// Assert that Strict satisfies the BackpressureConcept
static_assert(BackpressureConcept<Strict<>>, "backpressure::Strict does not satisfy BackpressureConcept");


// ------------------------------------------------------------
// Relaxed
// ------------------------------------------------------------
// Delayed activation
// Stabilized recovery

template<
    std::uint32_t ActivateThreshold   = config::backpressure::RELAXED_HYSTERESIS_ACTIVATION_THRESHOLD,
    std::uint32_t DeactivateThreshold = config::backpressure::RELAXED_HYSTERESIS_DEACTIVATION_THRESHOLD
>
requires (ActivateThreshold > 0) &&
         (DeactivateThreshold > 0)
struct Relaxed {

    static constexpr BackpressureMode mode = BackpressureMode::Relaxed;

    using hysteresis = lcr::control::BinaryHysteresis<ActivateThreshold, DeactivateThreshold>;

    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // ------------------------------------------------------------

    static constexpr const char* mode_name() noexcept {
        return "Relaxed";
    }

    static void dump(std::ostream& os) {
        os << "[Transport Backpressure Policy]\n";
        os << "- Mode        : " << mode_name() << "\n";
        os << "- Hysteresis  : enabled\n";
        os << "- Activation  : " << ActivateThreshold << " (threshold)\n";
        os << "- Deactivation: " << DeactivateThreshold << " (threshold)\n";
        os << "- Behavior    : Delayed activation, stabilized recovery\n\n";
    }
};

// Assert that Relaxed satisfies the BackpressureConcept
static_assert(BackpressureConcept<Relaxed<>>, "backpressure::Relaxed does not satisfy BackpressureConcept");

} // namespace backpressure


// ============================================================================
// Default Liveness Policy
// ============================================================================

using DefaultBackpressure = backpressure::Strict<>;

// Assert that DefaultBackpressure satisfies the BackpressureConcept
static_assert(BackpressureConcept<DefaultBackpressure>, "DefaultBackpressure does not satisfy BackpressureConcept");

} // namespace wirekrak::core::policy::transport

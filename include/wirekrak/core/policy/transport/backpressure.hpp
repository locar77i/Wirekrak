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
    { P::mode }  -> std::convertible_to<BackpressureMode>;
    { P::spins } -> std::convertible_to<std::uint32_t>;
    typename P::hysteresis;
}
&& (P::spins >= 1);

template<typename P>
concept BackpressureConcept =
    HasBackpressureMembers<P>
    &&
    (
        // ZeroTolerance -> no hysteresis allowed
        (
            P::mode == BackpressureMode::ZeroTolerance &&
            std::same_as<typename P::hysteresis, std::nullptr_t>
        )
        ||
        // Any other mode -> hysteresis required
        (
            P::mode != BackpressureMode::ZeroTolerance &&
            !std::same_as<typename P::hysteresis, std::nullptr_t>
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
    static constexpr std::uint32_t spins = 1;
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
        os << "- Spins       : " << spins << "\n";
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

struct Strict {

    static constexpr BackpressureMode mode = BackpressureMode::Strict;
    static constexpr std::uint32_t spins = config::backpressure::STRICT_LOCAL_SPINS;
    using hysteresis =
        lcr::control::BinaryHysteresis<
            config::backpressure::STRICT_HYSTERESIS_ACTIVATION_THRESHOLD,
            config::backpressure::STRICT_HYSTERESIS_DEACTIVATION_THRESHOLD
        >;

    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // ------------------------------------------------------------

    static constexpr const char* mode_name() noexcept {
        return "Strict";
    }

    static void dump(std::ostream& os) {
        os << "[Transport Backpressure Policy]\n";
        os << "- Mode        : " << mode_name() << "\n";
        os << "- Spins       : " << spins << "\n";
        os << "- Hysteresis  : enabled\n";
        os << "- Activation  : " << hysteresis::activate_threshold << " (threshold)\n";
        os << "- Deactivation: " << hysteresis::deactivate_threshold << " (threshold)\n";
        os << "- Behavior    : Immediate activation, stabilized recovery\n\n";
    }
};

// Assert that Strict satisfies the BackpressureConcept
static_assert(BackpressureConcept<Strict>, "backpressure::Strict does not satisfy BackpressureConcept");


// ------------------------------------------------------------
// Relaxed
// ------------------------------------------------------------
// Delayed activation
// Stabilized recovery

struct Relaxed {

    static constexpr BackpressureMode mode = BackpressureMode::Relaxed;
    static constexpr std::uint32_t spins = config::backpressure::RELAXED_LOCAL_SPINS;
    using hysteresis =
        lcr::control::BinaryHysteresis<
            config::backpressure::RELAXED_HYSTERESIS_ACTIVATION_THRESHOLD,
            config::backpressure::RELAXED_HYSTERESIS_DEACTIVATION_THRESHOLD
        >;

    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // ------------------------------------------------------------

    static constexpr const char* mode_name() noexcept {
        return "Relaxed";
    }

    static void dump(std::ostream& os) {
        os << "[Transport Backpressure Policy]\n";
        os << "- Mode        : " << mode_name() << "\n";
        os << "- Spins       : " << spins << "\n";
        os << "- Hysteresis  : enabled\n";
        os << "- Activation  : " << hysteresis::activate_threshold << " (threshold)\n";
        os << "- Deactivation: " << hysteresis::deactivate_threshold << " (threshold)\n";
        os << "- Behavior    : Delayed activation, stabilized recovery\n\n";
    }
};

// Assert that Relaxed satisfies the BackpressureConcept
static_assert(BackpressureConcept<Relaxed>, "backpressure::Relaxed does not satisfy BackpressureConcept");


// ------------------------------------------------------------
// Custom
// ------------------------------------------------------------

template<
    std::uint32_t Spins,
    std::uint32_t Activate,
    std::uint32_t Deactivate
>
requires (Spins >= 1) && (Activate >= 1) && (Deactivate >= 1)
struct Custom
{
    static constexpr BackpressureMode mode = BackpressureMode::Custom;
    static constexpr std::uint32_t spins = Spins;
    using hysteresis = lcr::control::BinaryHysteresis<Activate, Deactivate>;
    
    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // ------------------------------------------------------------

    static constexpr const char* mode_name() noexcept {
        return "Custom";
    }

    static void dump(std::ostream& os) {
        os << "[Transport Backpressure Policy]\n";
        os << "- Mode        : " << mode_name() << "\n";
        os << "- Spins       : " << spins << "\n";
        os << "- Hysteresis  : enabled\n";
        os << "- Activation  : " << hysteresis::activate_threshold << " (threshold)\n";
        os << "- Deactivation: " << hysteresis::deactivate_threshold << " (threshold)\n";
        os << "- Behavior    : Customized\n\n";
    }
};

// Assert that Custom satisfies the BackpressureConcept
static_assert(BackpressureConcept<Custom<1, 1, 1>>, "backpressure::Custom does not satisfy BackpressureConcept");

} // namespace backpressure


// ============================================================================
// Default Backpressure Policy
// ============================================================================

using DefaultBackpressure = backpressure::Strict;

// Assert that DefaultBackpressure satisfies the BackpressureConcept
static_assert(BackpressureConcept<DefaultBackpressure>, "DefaultBackpressure does not satisfy BackpressureConcept");

} // namespace wirekrak::core::policy::transport

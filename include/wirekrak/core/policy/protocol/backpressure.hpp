#pragma once

#include <concepts>
#include <ostream>

#include "wirekrak/core/config/backpressure.hpp"
#include "wirekrak/core/policy/backpressure_mode.hpp"


namespace wirekrak::core::policy::protocol {

// ============================================================================
// Backpressure Policy Concept
// ============================================================================

template<typename P>
concept HasBackpressureMembers =
requires {
    { P::mode } -> std::same_as<const BackpressureMode&>;
    { P::escalation_threshold } -> std::same_as<const std::uint32_t&>;
};

template<typename P>
concept BackpressureConcept =
    HasBackpressureMembers<P>
    &&
    (
        // ZeroTolerance → threshold must be exactly 1
        (
            P::mode == BackpressureMode::ZeroTolerance &&
            P::escalation_threshold == 1
        )
        ||
        // Strict / Relaxed → threshold must be > 0
        (
            (P::mode == BackpressureMode::Strict || P::mode == BackpressureMode::Relaxed)
            &&
            (P::escalation_threshold > 0)
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

    static constexpr std::uint32_t escalation_threshold = 1;

    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // ------------------------------------------------------------

    static constexpr const char* mode_name() noexcept {
        return "ZeroTolerance";
    }

    static void dump(std::ostream& os) {
        os << "[Protocol Backpressure Policy]\n";
        os << "- Mode        : " << mode_name() << "\n";
        os << "- Escalation  : " << escalation_threshold << " (threshold)\n";
        os << "- Behavior    : Immediate activation\n\n";
    }

};

// Assert that ZeroTolerance satisfies the BackpressureConcept
static_assert(BackpressureConcept<ZeroTolerance>, "ZeroTolerance does not satisfy BackpressureConcept");


// ------------------------------------------------------------
// Strict
// ------------------------------------------------------------
// Immediate activation
// Stabilized recovery

template<
    std::uint32_t EscalationThreshold = config::backpressure::STRICT_ESCALATION_THRESHOLD
>
struct Strict {

    static constexpr BackpressureMode mode = BackpressureMode::Strict;

    static constexpr std::uint32_t escalation_threshold = EscalationThreshold;

    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // ------------------------------------------------------------

    static constexpr const char* mode_name() noexcept {
        return "Strict";
    }

    static void dump(std::ostream& os) {
        os << "[Protocol Backpressure Policy]\n";
        os << "- Mode        : " << mode_name() << "\n";
        os << "- Escalation  : " << escalation_threshold << " (threshold)\n";
        os << "- Behavior    : Slightly delayed activation\n\n";
    }
};

// Assert that Strict satisfies the BackpressureConcept
static_assert(BackpressureConcept<Strict<>>, "Strict does not satisfy BackpressureConcept");


// ------------------------------------------------------------
// Relaxed
// ------------------------------------------------------------
// Delayed activation
// Stabilized recovery

template<
    std::uint32_t EscalationThreshold = config::backpressure::RELAXED_ESCALATION_THRESHOLD
>
struct Relaxed {

    static constexpr BackpressureMode mode = BackpressureMode::Relaxed;

    static constexpr std::uint32_t escalation_threshold = EscalationThreshold;

    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // ------------------------------------------------------------

    static constexpr const char* mode_name() noexcept {
        return "Relaxed";
    }

    static void dump(std::ostream& os) {
        os << "[Protocol Backpressure Policy]\n";
        os << "- Mode        : " << mode_name() << "\n";
        os << "- Escalation  : " << escalation_threshold << " (threshold)\n";
        os << "- Behavior    : Delayed activation\n\n";
    }
};

// Assert that Relaxed satisfies the BackpressureConcept
static_assert(BackpressureConcept<Relaxed<>>, "Relaxed does not satisfy BackpressureConcept");

} // namespace backpressure


// ----------------------------------------------------------------------------
// Default
// ----------------------------------------------------------------------------

using DefaultBackpressure = backpressure::Strict<>;
// Assert that DefaultBackpressure satisfies the BackpressureConcept
static_assert(BackpressureConcept<DefaultBackpressure>, "DefaultBackpressure does not satisfy BackpressureConcept");

} // namespace wirekrak::core::policy::protocol

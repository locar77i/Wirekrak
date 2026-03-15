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
    { P::mode }                 -> std::convertible_to<BackpressureMode>;
    { P::escalation_threshold } -> std::convertible_to<std::uint32_t>;
};

template<typename P>
concept BackpressureConcept =
    HasBackpressureMembers<P>
    &&
    (
        // ZeroTolerance -> threshold must be exactly 1
        (
            P::mode == BackpressureMode::ZeroTolerance &&
            P::escalation_threshold == 1
        )
        ||
        // Any other mode -> threshold must be greater than 1
        (
            P::mode != BackpressureMode::ZeroTolerance &&
            P::escalation_threshold > 1
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

struct Strict {

    static constexpr BackpressureMode mode = BackpressureMode::Strict;

    static constexpr std::uint32_t escalation_threshold = config::backpressure::STRICT_ESCALATION_THRESHOLD;

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
static_assert(BackpressureConcept<Strict>, "Strict does not satisfy BackpressureConcept");


// ------------------------------------------------------------
// Relaxed
// ------------------------------------------------------------
// Delayed activation
// Stabilized recovery

struct Relaxed {

    static constexpr BackpressureMode mode = BackpressureMode::Relaxed;

    static constexpr std::uint32_t escalation_threshold = config::backpressure::RELAXED_ESCALATION_THRESHOLD;

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
static_assert(BackpressureConcept<Relaxed>, "Relaxed does not satisfy BackpressureConcept");


// ------------------------------------------------------------
// Custom
// ------------------------------------------------------------
// Fully user-defined escalation threshold

template<std::uint32_t EscalationThreshold>
requires (EscalationThreshold > 1)
struct Custom {

    static constexpr BackpressureMode mode = BackpressureMode::Custom;

    static constexpr std::uint32_t escalation_threshold = EscalationThreshold;

    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // ------------------------------------------------------------

    static constexpr const char* mode_name() noexcept {
        return "Custom";
    }

    static void dump(std::ostream& os) {
        os << "[Protocol Backpressure Policy]\n";
        os << "- Mode        : " << mode_name() << "\n";
        os << "- Escalation  : " << escalation_threshold << " (threshold)\n";
        os << "- Behavior    : Customized\n\n";
    }
};

// Assert that Custom satisfies the BackpressureConcept
static_assert(BackpressureConcept<Custom<2>>, "Custom does not satisfy BackpressureConcept");

} // namespace backpressure


// ----------------------------------------------------------------------------
// Default
// ----------------------------------------------------------------------------

using DefaultBackpressure = backpressure::Strict;
// Assert that DefaultBackpressure satisfies the BackpressureConcept
static_assert(BackpressureConcept<DefaultBackpressure>, "DefaultBackpressure does not satisfy BackpressureConcept");

} // namespace wirekrak::core::policy::protocol

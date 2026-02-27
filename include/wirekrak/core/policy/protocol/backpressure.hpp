#pragma once

#include <concepts>

#include "wirekrak/core/config/backpressure.hpp"
#include "wirekrak/core/policy/backpressure_mode.hpp"


namespace wirekrak::core::policy::protocol {

// ============================================================================
// Backpressure Policy Concept
// ============================================================================

template<typename P>
concept BackpressurePolicy =
requires {
    { P::mode } -> std::same_as<const BackpressureMode&>;
     { P::escalation_threshold } -> std::convertible_to<std::uint32_t>;
};

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

};


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

};


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

};

} // namespace backpressure

} // namespace wirekrak::core::policy::protocol

#pragma once

#include <concepts>

#include "wirekrak/core/config/backpressure.hpp"
#include "wirekrak/core/policy/backpressure_mode.hpp"
#include "lcr/control/binary_hysteresis.hpp"


namespace wirekrak::core::policy::transport {

// ============================================================================
// Backpressure Policy Concept
// ============================================================================

template<typename P>
concept BackpressurePolicy =
requires {
    { P::mode } -> std::same_as<const BackpressureMode&>;
    typename P::hysteresis;
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

    using hysteresis = std::nullptr_t;
};


// ------------------------------------------------------------
// Strict
// ------------------------------------------------------------
// Immediate activation
// Stabilized recovery

template<
    std::uint32_t DeactivateThreshold = config::backpressure::STRICT_HYSTERESIS_DEACTIVATION_THRESHOLD
>
struct Strict {

    static constexpr BackpressureMode mode = BackpressureMode::Strict;

    using hysteresis = lcr::control::BinaryHysteresis<config::backpressure::STRICT_HYSTERESIS_ACTIVATION_THRESHOLD, DeactivateThreshold>;
};


// ------------------------------------------------------------
// Relaxed
// ------------------------------------------------------------
// Delayed activation
// Stabilized recovery

template<
    std::uint32_t ActivateThreshold   = config::backpressure::RELAXED_HYSTERESIS_ACTIVATION_THRESHOLD,
    std::uint32_t DeactivateThreshold = config::backpressure::RELAXED_HYSTERESIS_DEACTIVATION_THRESHOLD
>
struct Relaxed {

    static constexpr BackpressureMode mode = BackpressureMode::Relaxed;

    using hysteresis = lcr::control::BinaryHysteresis<ActivateThreshold, DeactivateThreshold>;
};

} // namespace backpressure

} // namespace wirekrak::core::policy::transport

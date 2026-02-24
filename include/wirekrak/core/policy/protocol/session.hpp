#pragma once

#include <concepts>

#include "wirekrak/core/policy/backpressure.hpp"
#include "wirekrak/core/policy/protocol/liveness.hpp"
#include "wirekrak/core/policy/protocol/symbol_limit.hpp"


namespace wirekrak::core::policy::protocol {

// ============================================================================
// WebSocket Policy Bundle
// ============================================================================
//
// This bundle acts as a single injection point for protocol behavior.
//
// Keeping this as a bundle prevents template parameter explosion.
// ============================================================================

template<
    BackpressurePolicy BackpressureT = backpressure::Strict,    // Family
    LivenessConcept LivenessT        = DefaultLiveness,         // Concept
    SymbolLimitConcept SymbolLimitT  = NoSymbolLimits           // Concept
>
struct session_bundle {

    using backpressure = BackpressureT;
    using liveness     = LivenessT;
    using symbol_limit = SymbolLimitT;

    // Future policy additions go here
};

// Default bundle alias
using SessionDefault = session_bundle<>;

} // namespace wirekrak::core::policy::protocol

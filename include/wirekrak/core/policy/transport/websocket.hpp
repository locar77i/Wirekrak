#pragma once

#include <concepts>

#include "wirekrak/core/policy/backpressure.hpp"

namespace wirekrak::core::policy::transport {

// ============================================================================
// WebSocket Policy Bundle
// ============================================================================
//
// This bundle acts as a single injection point for transport behavior.
//
// Future additions may include:
//
//   using ClosePolicy;
//   using FragmentPolicy;
//   using LivenessPolicy;
//   using TimeoutPolicy;
//
// Keeping this as a bundle prevents template parameter explosion.
// ============================================================================

template<
    BackpressurePolicy BackpressureT = backpressure::ZeroTolerance
>
struct websocket_bundle {

    using backpressure = BackpressureT;

    // Future policy additions go here
};

// Default bundle alias
using WebsocketDefault = websocket_bundle<>;

} // namespace wirekrak::core::policy::transport
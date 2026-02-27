#pragma once

#include <concepts>

#include "wirekrak/core/policy/transport/backpressure.hpp"

namespace wirekrak::core::policy::transport {

// ============================================================================
// WebSocket Policy Bundle
// ============================================================================
//
// Single injection point for transport behavior.
// Prevents template parameter explosion.
//
// The bundle forwards:
//   - backpressure policy
//   - mode
//   - hysteresis type (if applicable)
//
// ZeroTolerance does not expose hysteresis.
// Strict / Relaxed do.
// ============================================================================

template<
    BackpressurePolicy BackpressureT = backpressure::Strict<>
>
struct websocket_bundle {

    using backpressure = BackpressureT;

    // Future policy additions go here
};

// Default bundle alias
using WebsocketDefault = websocket_bundle<>;

} // namespace wirekrak::core::policy::transport

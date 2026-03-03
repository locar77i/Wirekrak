#pragma once

/*
===============================================================================
 wirekrak::core::policy::transport::WebSocketBundleConcept
===============================================================================

Defines the policy bundle and bundle concept for WebSocket transport behavior.

The WebSocket transport owns:

  • Low-level send/receive mechanics
  • Fragment assembly
  • Backpressure detection
  • Transport-level error signaling

To prevent template parameter explosion and preserve API clarity, all
WebSocket-level policies are grouped into a single bundle type.

-------------------------------------------------------------------------------
 Why a Bundle Concept?
-------------------------------------------------------------------------------

Instead of validating nested members via ad-hoc requires clauses, this concept:

  • Ensures structural correctness (required nested types exist)
  • Enforces semantic correctness (nested types satisfy policy concepts)
  • Produces clearer compile-time diagnostics
  • Keeps WebSocket template declarations clean and readable

-------------------------------------------------------------------------------
 Required Nested Types
-------------------------------------------------------------------------------

A valid WebSocketBundleConcept must define:

    using backpressure;

And that type must satisfy:

    BackpressureConcept

-------------------------------------------------------------------------------
 Design Guarantees
-------------------------------------------------------------------------------

• Fully compile-time configuration
• Zero runtime polymorphism
• Zero dynamic configuration
• Deterministic per WebSocket type
• Extensible for future transport policies

===============================================================================
*/

#include <concepts>

#include "wirekrak/core/policy/transport/backpressure.hpp"


namespace wirekrak::core::policy::transport {

// -----------------------------------------------------------------------------
// Structural validation
// -----------------------------------------------------------------------------

template<typename T>
concept HasWebSocketMembers =
requires {
    typename T::backpressure;
};

// -----------------------------------------------------------------------------
// Semantic validation
// -----------------------------------------------------------------------------

template<typename T>
concept WebSocketBundleConcept =
    HasWebSocketMembers<T> &&
    BackpressureConcept<typename T::backpressure>;


// ============================================================================
// WebSocket Policy Bundle
// ============================================================================
//
// Groups WebSocket-level transport policies into a single injection point.
//
// Future extensions may include:
//   - frame size limits
//   - fragmentation strategy
//   - send throttling
//   - control ring overflow behavior
//
// ============================================================================

template<
    BackpressureConcept BackpressureT = DefaultBackpressure
>
struct websocket_bundle {

    using backpressure = BackpressureT;

    // Future WebSocket-level policies go here
};


// ============================================================================
// Default Bundle
// ============================================================================

using WebsocketDefault = websocket_bundle<>;

// Compile-time self-check
static_assert(WebSocketBundleConcept<WebsocketDefault>, "WebsocketDefault does not satisfy WebSocketBundleConcept");

} // namespace wirekrak::core::policy::transport
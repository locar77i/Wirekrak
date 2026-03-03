#pragma once

/*
===============================================================================
 wirekrak::core::policy::transport::ConnectionBundleConcept
===============================================================================

Defines the policy bundle and bundle concept for transport-level Connection.

The Connection represents a logical transport connection and owns:

  • WebSocket lifecycle
  • Reconnection strategy
  • Liveness monitoring
  • Observable connection signals

To prevent template parameter explosion and preserve API clarity, all
connection-level policies are grouped into a single bundle type.

-------------------------------------------------------------------------------
 Why a Bundle Concept?
-------------------------------------------------------------------------------

Instead of validating nested members via ad-hoc requires clauses, this concept:

  • Ensures structural correctness (required nested types exist)
  • Enforces semantic correctness (nested types satisfy policy concepts)
  • Produces clearer compile-time diagnostics
  • Keeps Connection template declarations clean

-------------------------------------------------------------------------------
 Required Nested Types
-------------------------------------------------------------------------------

A valid ConnectionBundleConcept must define:

    using liveness;

And that type must satisfy:

    LivenessConcept

-------------------------------------------------------------------------------
 Design Guarantees
-------------------------------------------------------------------------------

• Fully compile-time configuration
• Zero runtime polymorphism
• Zero dynamic configuration
• Deterministic per Connection type
• Extensible for future transport policies

===============================================================================
*/

#include <concepts>

#include "wirekrak/core/policy/transport/liveness.hpp"


namespace wirekrak::core::policy::transport {

// -----------------------------------------------------------------------------
// Structural validation
// -----------------------------------------------------------------------------

template<typename T>
concept HasConnectionBundleMembers =
requires {
    typename T::liveness;
};

// -----------------------------------------------------------------------------
// Semantic validation
// -----------------------------------------------------------------------------

template<typename T>
concept ConnectionBundleConcept =
    HasConnectionBundleMembers<T> &&
    LivenessConcept<typename T::liveness>;


// ============================================================================
// Connection Policy Bundle
// ============================================================================
//
// Groups transport-level policies into a single injection point.
//
// Future extensions may include:
//   - retry/backoff policy
//   - jitter strategy
//   - reconnection caps
//   - idle behavior
//   - signal overflow handling
//
// ============================================================================

template<
    LivenessConcept LivenessT = DefaultLiveness
>
struct connection_bundle {

    using liveness = LivenessT;

    // Future connection-level policies go here
};


// ============================================================================
// Default Bundle
// ============================================================================

using ConnectionDefault = connection_bundle<>;

// Compile-time self-check
static_assert(ConnectionBundleConcept<ConnectionDefault>, "ConnectionDefault does not satisfy ConnectionBundleConcept");

} // namespace wirekrak::core::policy::transport

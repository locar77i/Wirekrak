#pragma once

/*
===============================================================================
 wirekrak::core::policy::transport::ConnectionBundleConcept
===============================================================================

Defines the policy bundle and bundle concept for transport-level Connection.

The Connection represents a logical transport connection and owns:

  • WebSocket lifecycle
  • Reconnection mode
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
#include <ostream>

#include "wirekrak/core/policy/transport/liveness.hpp"
#include "wirekrak/core/policy/transport/retry.hpp"


namespace wirekrak::core::policy::transport {

// -----------------------------------------------------------------------------
// Structural validation
// -----------------------------------------------------------------------------

template<typename T>
concept HasConnectionBundleMembers =
requires {
    typename T::liveness;
    typename T::retry;
};

// -----------------------------------------------------------------------------
// Semantic validation
// -----------------------------------------------------------------------------

template<typename T>
concept ConnectionBundleConcept =
    HasConnectionBundleMembers<T> &&
    LivenessConcept<typename T::liveness> &&
    RetryConcept<typename T::retry>;

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
    LivenessConcept LivenessT = DefaultLiveness,
    RetryConcept RetryT = DefaultRetry
>
struct connection_bundle {

    using liveness = LivenessT;
    using retry = RetryT;

    // Future connection-level policies go here

    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // No instances, fully compile-time, removed entirely if unused
    // ------------------------------------------------------------

    static void dump(std::ostream& os) {
        os << "\n=== Transport Connection Policies ===\n";
        liveness::dump(os);
        retry::dump(os);
    }
};


// ============================================================================
// Default Bundle
// ============================================================================

using DefaultConnection = connection_bundle<>;

// Compile-time self-check
static_assert(ConnectionBundleConcept<DefaultConnection>, "DefaultConnection does not satisfy ConnectionBundleConcept");

} // namespace wirekrak::core::policy::transport

#pragma once

/*
===============================================================================
 wirekrak::core::policy::protocol::SessionBundleConcept Concept
===============================================================================

This header defines the SessionBundleConcept concept.

A SessionBundleConcept represents the compile-time injection point for all
protocol-level behavioral policies used by a Kraken Session.

The bundle groups related policy families into a single template parameter
to prevent template parameter explosion and preserve API clarity.

-------------------------------------------------------------------------------
 Why a Bundle Concept?
-------------------------------------------------------------------------------

Instead of validating individual nested types via ad-hoc requires clauses,
this concept:

  • Ensures structural correctness (required nested types exist)
  • Enforces semantic correctness (nested types satisfy their policy concepts)
  • Produces clearer compile-time diagnostics
  • Keeps Session template declarations clean and readable

-------------------------------------------------------------------------------
 Required Nested Types
-------------------------------------------------------------------------------

A valid SessionBundleConcept must define:

    using backpressure;
    using liveness;
    using symbol_limit;

And each must satisfy the corresponding policy concept:

    backpressure  -> BackpressurePolicy
    liveness      -> LivenessConcept
    symbol_limit  -> SymbolLimitConcept

-------------------------------------------------------------------------------
 Example
-------------------------------------------------------------------------------

using MyBundle = session_bundle<
    backpressure::Strict<>,
    DefaultLiveness,
    NoSymbolLimits
>;

static_assert(SessionBundleConcept<MyBundle>);

-------------------------------------------------------------------------------
 Design Guarantees
-------------------------------------------------------------------------------

• Zero runtime overhead
• Pure compile-time validation
• No inheritance required
• No virtual dispatch
• Extensible for future policy additions

===============================================================================
*/

#include <concepts>
#include <ostream>

#include "wirekrak/core/policy/protocol/backpressure.hpp"
#include "wirekrak/core/policy/protocol/liveness.hpp"
#include "wirekrak/core/policy/protocol/symbol_limit.hpp"


namespace wirekrak::core::policy::protocol {

// -----------------------------------------------------------------------------
// Structural validation: required nested policy members
// -----------------------------------------------------------------------------

template<typename T>
concept HasSessionBundleMembers =
    requires {
        typename T::backpressure;
        typename T::liveness;
        typename T::symbol_limit;
    };


// -----------------------------------------------------------------------------
// Semantic validation: nested types satisfy required policy concepts
// -----------------------------------------------------------------------------

template<typename T>
concept SessionBundleConcept =
    HasSessionBundleMembers<T> &&
    BackpressureConcept<typename T::backpressure> &&
    LivenessConcept<typename T::liveness> &&
    SymbolLimitConcept<typename T::symbol_limit>;



// ============================================================================
// WebSocket Policy Bundle
// ============================================================================
//
// This bundle acts as a single injection point for protocol behavior.
//
// Keeping this as a bundle prevents template parameter explosion.
// ============================================================================

template<
    BackpressureConcept BackpressureT = backpressure::Strict<>,
    LivenessConcept LivenessT        = DefaultLiveness,
    SymbolLimitConcept SymbolLimitT  = NoSymbolLimits
>
struct session_bundle {

    using backpressure = BackpressureT;
    using liveness     = LivenessT;
    using symbol_limit = SymbolLimitT;

    // Future policy additions go here

    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // No instances, fully compile-time, removed entirely if unused
    // ------------------------------------------------------------

    static void dump(std::ostream& os) {
        os << "\n=== Protocol Session Policies ===\n";
        backpressure::dump(os);
        liveness::dump(os);
        symbol_limit::dump(os);
    }
};

// ============================================================================
// Default Bundle
// ============================================================================

using SessionDefault = session_bundle<>;

// Assert that SessionDefault satisfies the SessionBundleConcept concept
static_assert(SessionBundleConcept<SessionDefault>, "SessionDefault does not satisfy SessionBundleConcept concept");

} // namespace wirekrak::core::policy::protocol

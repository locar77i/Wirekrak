#pragma once

// ============================================================================
// Replay Traits (Protocol → Persistent Intent Mapping)
// ============================================================================
//
// The replay_traits define how protocol-level request types map to the
// corresponding **persistent subscription intent** stored in the Replay
// Database.
//
// This is a pure compile-time mechanism that decouples:
//
//   • Protocol messages        (Subscribe / Unsubscribe / etc.)
//   • Persistent state         (what must be replayed after reconnect)
//
// ---------------------------------------------------------------------------
// Design goals
// ---------------------------------------------------------------------------
// • Protocol-driven mapping
//     - Each protocol defines how its request types translate to replayable
//       subscription state
//
// • Asymmetric request support
//     - Different request types may map to the same persistent intent
//       (e.g. Unsubscribe → Subscribe)
//
// • Zero runtime overhead
//     - Fully resolved at compile time via template specialization
//
// • Extensible per exchange
//     - Each exchange provides its own specializations
//     - No changes required in core replay infrastructure
//
// ---------------------------------------------------------------------------
// Usage
// ---------------------------------------------------------------------------
// • replay_type<RequestT>
//     Resolves to the subscription type associated with RequestT
//
// Example:
//     replay_type<trade::Subscribe>   → trade::Subscribe
//     replay_type<trade::Unsubscribe> → trade::Subscribe
//
// ---------------------------------------------------------------------------
// Requirements
// ---------------------------------------------------------------------------
// • Must be specialized for every request type interacting with the
//   Replay Database
//
// • Specializations must define:
//       using type = <subscription_type>;
//
// ---------------------------------------------------------------------------
// Notes
// ---------------------------------------------------------------------------
// • This trait is part of the **protocol correctness layer**
// • It does NOT encode transport, parsing, or data-plane behavior
// • It strictly defines *what state must survive reconnects*
//
// ============================================================================

namespace wirekrak::core::protocol {

// Primary template (must be specialized by protocol)
template<class RequestT>
struct replay_traits {
    static_assert(sizeof(RequestT) == 0, "replay_traits<RequestT> must be specialized for this type");
};

template<class RequestT>
using replay_type = typename replay_traits<RequestT>::type;

} // namespace wirekrak::core::protocol

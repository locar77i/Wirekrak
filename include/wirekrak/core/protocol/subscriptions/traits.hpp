#pragma once

// ============================================================================
// Subscription Traits (Protocol → State Ownership Mapping)
// ============================================================================
//
// The subscription_traits define how protocol-level message types map to their
// corresponding **state-owning subscription type**.
//
// This is a pure compile-time mechanism that decouples:
//
//   • Protocol messages        (Subscribe / Unsubscribe / ACKs / Rejections)
//   • Subscription state       (the canonical owner of symbol lifecycle)
//
// ---------------------------------------------------------------------------
// Design goals
// ---------------------------------------------------------------------------
// • Unified state mapping
//     - All protocol messages that affect subscription state resolve to a
//       single owning subscription type
//
// • Asymmetric request support
//     - Different message types may map to the same state owner
//       (e.g. Unsubscribe → Subscribe, SubscribeAck → Subscribe)
//
// • Zero runtime overhead
//     - Fully resolved at compile time via template specialization
//
// • Extensible per exchange
//     - Each exchange provides its own specializations
//     - No changes required in core subscription or replay infrastructure
//
// ---------------------------------------------------------------------------
// Usage
// ---------------------------------------------------------------------------
// • subscription_type<T>
//     Resolves to the subscription type that owns the state for T
//
// Example:
//     subscription_type<trade::Subscribe>        → trade::Subscribe
//     subscription_type<trade::Unsubscribe>      → trade::Subscribe
//     subscription_type<trade::SubscribeAck>     → trade::Subscribe
//     subscription_type<trade::UnsubscribeAck>   → trade::Subscribe
//
// ---------------------------------------------------------------------------
// Requirements
// ---------------------------------------------------------------------------
// • Must be specialized for every protocol type that interacts with
//   subscription state (requests, ACKs, rejections, etc.)
//
// • Specializations must define:
//       using type = <subscription_type>;
//
// ---------------------------------------------------------------------------
// Notes
// ---------------------------------------------------------------------------
// • This trait is part of the **protocol correctness layer**
// • It is used by:
//     - Replay Database (persistent intent)
//     - Subscription Controller (state coordination)
//     - ACK / rejection routing (state mutation)
//
// • It does NOT encode transport, parsing, or data-plane behavior
// • It strictly defines *which type owns subscription state*
//
// ============================================================================

namespace wirekrak::core::protocol {

// Primary template (must be specialized by protocol)
template<class RequestT>
struct subscription_traits {
    static_assert(sizeof(RequestT) == 0, "subscription_traits<RequestT> must be specialized for this type");
};

template<class RequestT>
using subscription_type = typename subscription_traits<RequestT>::type;

} // namespace wirekrak::core::protocol

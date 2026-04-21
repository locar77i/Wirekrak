#pragma once

// ============================================================================
// Subscription Set (Protocol Subscription Domains)
// ============================================================================
//
// Defines the complete set of **subscription root types** supported by a
// protocol.
//
// A SubscriptionSet is a pure compile-time construct that enumerates the
// canonical subscription types that own state within the system.
//
// ---------------------------------------------------------------------------
// Design goals
// ---------------------------------------------------------------------------
// • Explicit domain definition
//     - Lists all subscription state owners (e.g. trade, book)
//
// • Zero runtime overhead
//     - Pure type-level container
//     - No instances, no allocation, no indirection
//
// • Protocol decoupling
//     - Removes hardcoded protocol types from core infrastructure
//     - Enables generic Session / Controller / ReplayDB
//
// • Compile-time validation
//     - Allows enforcing that all subscription-related messages resolve
//       to a known subscription domain
//
// ---------------------------------------------------------------------------
// Semantics
// ---------------------------------------------------------------------------
//
// • Each type in `types` MUST represent a **subscription root**
//     (i.e. the canonical owner of symbol lifecycle)
//
// • These are typically:
//     - Subscribe request types
//     - One per logical channel/domain
//
// • The SubscriptionSet does NOT:
//     - Define message parsing
//     - Define transport behavior
//     - Define ownership mapping (see subscription_traits)
//     - Contain runtime state
//
// ---------------------------------------------------------------------------
// Relationship with subscription_traits
// ---------------------------------------------------------------------------
//
// subscription_traits<T> defines:
//
//     Protocol message  →  owning subscription type
//
// SubscriptionSet defines:
//
//     Set of all owning subscription types
//
// Together they ensure:
//
//     • Every protocol message maps to a valid subscription domain
//     • All domains are known at compile time
//
// ---------------------------------------------------------------------------
// Example
// ---------------------------------------------------------------------------
//
// struct KrakenSubscriptionSet {
//     using types = type_list<
//         kraken::schema::trade::Subscribe,
//         kraken::schema::book::Subscribe
//     >;
// };
//
// ---------------------------------------------------------------------------
// Usage
// ---------------------------------------------------------------------------
//
// using Controller = subscription::Controller<ProgressPolicy, SubscriptionSet>;
//
// using ReplayDB   = replay::Database<SubscriptionSet>;
//
// ---------------------------------------------------------------------------
// Requirements
// ---------------------------------------------------------------------------
//
// • Must define:
//
//       using types = type_list<...>;
//
// • All types must be unique
//
// • Each type must be a valid subscription root recognized by
//   subscription_traits
//
// ---------------------------------------------------------------------------
// Notes
// ---------------------------------------------------------------------------
//
// • This is a structural definition, not a behavior trait
//
// • Keep this minimal and stable — avoid adding unrelated protocol concerns
//
// • Additional protocol features (parsing, routing, etc.) should NOT be added
//   here — introduce separate abstractions instead
//
// ============================================================================

namespace wirekrak::core::protocol {

// Primary template (intentionally undefined)
//
// A protocol must provide a concrete SubscriptionSet type.
// This template exists only for documentation and constraint purposes.
template<class T>
struct SubscriptionSet;

} // namespace wirekrak::core::protocol

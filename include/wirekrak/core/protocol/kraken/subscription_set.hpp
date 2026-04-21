#pragma once

// ============================================================================
// Kraken Subscription Set
// ============================================================================
//
// Defines the set of **subscription state types** supported by the Kraken
// protocol.
//
// This acts as a compile-time registry of all subscription domains that:
//
//   • Own symbol lifecycle state
//   • Participate in SubscriptionController
//   • Participate in ReplayDB
//
// ---------------------------------------------------------------------------
// Design goals
// ---------------------------------------------------------------------------
// • Centralized domain definition
//     - All subscription-owning types are declared in one place
//
// • Decoupling from core
//     - Core Session, Controller, and ReplayDB depend on this set,
//       not on protocol-specific types directly
//
// • Compile-time only
//     - No runtime overhead
//     - Fully resolved via templates
//
// • Extensible
//     - New channels can be added by extending the type list
//     - No changes required in generic infrastructure
//
// ---------------------------------------------------------------------------
// What belongs here?
// ---------------------------------------------------------------------------
//
// Only **state-owning subscription types**, typically:
//
//   • trade::Subscribe
//   • book::Subscribe
//
// NOT included:
//
//   • Unsubscribe
//   • ACK messages
//   • Rejection messages
//
// Those are mapped via `subscription_traits` to these types.
//
// ---------------------------------------------------------------------------
// Relationship with subscription_traits
// ---------------------------------------------------------------------------
//
// subscription_traits<T> defines:
//
//     T → owning subscription type
//
// SubscriptionSet defines:
//
//     The set of ALL owning subscription types
//
// Together they allow:
//
//   • Generic routing of ACKs / rejections
//   • ReplayDB to be protocol-agnostic
//   • SubscriptionController to manage multiple domains uniformly
//
// ---------------------------------------------------------------------------

#include "wirekrak/core/meta/type_list.hpp"

#include "wirekrak/core/protocol/subscription_traits.hpp"

#include "wirekrak/core/protocol/kraken/schema/trade/subscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/subscribe.hpp"


namespace wirekrak::core::protocol::kraken {

// ------------------------------------------------------------
// SubscriptionSet
// ------------------------------------------------------------

struct SubscriptionSet {
    using types = wirekrak::core::meta::type_list<
        schema::trade::Subscribe,
        schema::book::Subscribe
    >;
};


// ------------------------------------------------------------
// Validation (compile-time)
// ------------------------------------------------------------
//
// Ensures that:
//
//   • Every type in SubscriptionSet has a subscription_traits specialization
//   • Each type maps to itself (i.e. is an owning subscription type)
//
// This prevents:
//
//   • Missing trait specializations
//   • Accidental inclusion of non-owning types (ACKs, Unsubscribe, etc.)
//
// Errors are triggered at compile-time, close to the source of the issue.
//

namespace detail {

// ------------------------------------------------------------
// Per-type validation
// ------------------------------------------------------------

template<class T>
struct validate_subscription_type {

    static_assert(
        requires { typename subscription_type<T>; },
        "Missing subscription_traits specialization for a Kraken subscription type"
    );

    static_assert(
        std::same_as<subscription_type<T>, T>,
        "Kraken SubscriptionSet must contain only owning subscription types"
    );
};


// ------------------------------------------------------------
// Set validation
// ------------------------------------------------------------

template<class List>
struct validate_subscription_set;

template<class... Ts>
struct validate_subscription_set<wirekrak::core::meta::type_list<Ts...>> {
    static constexpr bool value =
        (validate_subscription_type<Ts>{}, ... , true);
};


// ------------------------------------------------------------
// Trigger validation
// ------------------------------------------------------------

static_assert(
    validate_subscription_set<SubscriptionSet::types>::value,
    "Invalid Kraken SubscriptionSet configuration"
);

} // namespace detail

} // namespace wirekrak::core::protocol::kraken

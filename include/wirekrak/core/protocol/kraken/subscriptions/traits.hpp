#pragma once

#include "wirekrak/core/protocol/subscriptions/traits.hpp"

#include "wirekrak/core/protocol/kraken/schema/trade/subscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/unsubscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/subscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/unsubscribe_ack.hpp"

#include "wirekrak/core/protocol/kraken/schema/book/subscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/unsubscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/subscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/unsubscribe_ack.hpp"


namespace wirekrak::core::protocol {

// ============================================================================
// SUBSCRIPTION TRAITS
// ============================================================================
//
// This trait is protocol-specific and must be specialized per exchange.
//
// ============================================================================


// ---------------------------------------------------------------------------
// TRADE channel
// ---------------------------------------------------------------------------

template<>
struct subscription_traits<kraken::schema::trade::Subscribe> {
    using type = kraken::schema::trade::Subscribe;
};

template<>
struct subscription_traits<kraken::schema::trade::Unsubscribe> {
    using type = kraken::schema::trade::Subscribe;
};

template<>
struct subscription_traits<kraken::schema::trade::SubscribeAck> {
    using type = kraken::schema::trade::Subscribe;
};

template<>
struct subscription_traits<kraken::schema::trade::UnsubscribeAck> {
    using type = kraken::schema::trade::Subscribe;
};

// ---------------------------------------------------------------------------
// BOOK channel
// ---------------------------------------------------------------------------

template<>
struct subscription_traits<kraken::schema::book::Subscribe> {
    using type = kraken::schema::book::Subscribe;
};

template<>
struct subscription_traits<kraken::schema::book::Unsubscribe> {
    using type = kraken::schema::book::Subscribe;
};

template<>
struct subscription_traits<kraken::schema::book::SubscribeAck> {
    using type = kraken::schema::book::Subscribe;
};

template<>
struct subscription_traits<kraken::schema::book::UnsubscribeAck> {
    using type = kraken::schema::book::Subscribe;
};

} // namespace wirekrak::core::protocol

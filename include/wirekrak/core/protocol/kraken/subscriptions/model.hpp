// subscription_model.hpp

#pragma once

#include "wirekrak/core/protocol/kraken/subscriptions/traits.hpp"
#include "wirekrak/core/protocol/kraken/subscriptions/set.hpp"


namespace wirekrak::core::protocol::kraken {

struct SubscriptionModel {

    // ------------------------------------------------------------
    // Canonical subscription types (domain types)
    // ------------------------------------------------------------
    using types = SubscriptionSet::types;

    // ------------------------------------------------------------
    // Request/Ack → canonical subscription mapping
    // ------------------------------------------------------------
    template<class T>
    using subscription_type_t = subscription_type<T>;
};

} // namespace wirekrak::core::protocol::kraken

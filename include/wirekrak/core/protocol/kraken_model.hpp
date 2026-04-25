#pragma once

/*
===============================================================================
Kraken Protocol Model
===============================================================================

The KrakenModel defines the complete protocol configuration at compile time.

It acts as the composition root for:
  • Subscription model (control-plane)
  • Message types (data-plane)
  • State types (state-plane)
  • Protocol message_handler (parsing + routing)

------------------------------------------------------------------------------
Design goals
------------------------------------------------------------------------------

• Zero runtime overhead
    - Pure type container (no instances required)
    - Fully resolved at compile time

• Strong separation of concerns
    - SubscriptionModel → control-plane
    - Messages          → data-plane
    - States            → state-plane
    - MessageHandler    → protocol logic

• Protocol encapsulation
    - No leakage of Kraken-specific details into generic Session

• Extensibility
    - New protocols can define their own model with the same structure

------------------------------------------------------------------------------
Usage
------------------------------------------------------------------------------

using SessionT = Session<
    KrakenModel,
    WS,
    MessageRing
>;

------------------------------------------------------------------------------
Structure
------------------------------------------------------------------------------

KrakenModel exposes:

    using subscription_model
    using messages
    using states
    using message_handler

------------------------------------------------------------------------------
Notes
------------------------------------------------------------------------------
- ACK messages are intentionally excluded. They are handled synchronously via
  Context and never reach the data-plane.
-------------------------------------------------------------------------------
*/

#include "wirekrak/core/meta/type_list.hpp"
#include "wirekrak/core/protocol/kraken/subscriptions/model.hpp"
#include "wirekrak/core/protocol/kraken/message_handler.hpp"
// Schema types (messages + states)
#include "wirekrak/core/protocol/kraken/schema/system/pong.hpp"
#include "wirekrak/core/protocol/kraken/schema/status/update.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/response.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/response.hpp"
#include "wirekrak/core/protocol/kraken/schema/rejection_notice.hpp"

namespace wirekrak::core::protocol {

// ============================================================================
// KrakenModel
// ============================================================================
struct KrakenModel {

    // =========================================================================
    // CONTROL PLANE
    // =========================================================================

    using subscription_model = kraken::SubscriptionModel;

    // =========================================================================
    // DATA PLANE (message streams)
    // =========================================================================

    using messages = meta::type_list<
        // Trade channel
        kraken::schema::trade::Response,

        // Book channel
        kraken::schema::book::Response,

        // Rejections
        kraken::schema::rejection::Notice
    >;

    // =========================================================================
    // STATE PLANE (latest-value storage)
    // =========================================================================

    using states = meta::type_list<
        kraken::schema::system::Pong,
        kraken::schema::status::Update
    >;

    // =========================================================================
    // PROTOCOL LOGIC
    // =========================================================================

    using message_handler = kraken::MessageHandler;
};

} // namespace wirekrak::core::protocol

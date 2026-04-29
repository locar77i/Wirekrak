#pragma once

/*
===============================================================================
Kraken Protocol MessageHandler
===============================================================================

This component implements the Kraken WebSocket message parsing and routing
logic for Wirekrak.

It is designed to be plugged into the generic protocol::Session via the
MessageHandlerConcept.

-------------------------------------------------------------------------------
Responsibilities
-------------------------------------------------------------------------------

The MessageHandler is responsible for:

  • Parsing raw WebSocket messages (JSON)
  • Identifying message type (event, channel, data, etc.)
  • Routing messages to the correct domain
  • Translating raw payloads into domain events
  • Emitting events via the Session Context

-------------------------------------------------------------------------------
Non-Responsibilities
-------------------------------------------------------------------------------

The MessageHandler MUST NOT:

  • Access Session internals directly
  • Manage subscriptions or replay state directly
  • Perform I/O or blocking operations
  • Allocate memory on the hot path (recommended)

All state mutations MUST go through the provided Context.

-------------------------------------------------------------------------------
Integration Contract
-------------------------------------------------------------------------------

The handler must satisfy:

  MessageResult  on_message(Context&, std::string_view) noexcept;

Where Context provides:

  • on_subscribe_ack(...)
  • on_unsubscribe_ack(...)
  • on_rejection(...)
  • push(...)

-------------------------------------------------------------------------------
Design Notes
-------------------------------------------------------------------------------

  • Zero runtime polymorphism (fully inlined)
  • Branch-based routing (can evolve to table-driven dispatch)
  • Parsing strategy is pluggable (simdjson recommended)
  • Safe to call inside tight polling loop

===============================================================================
*/

#pragma once

#include <string_view>

#include "wirekrak/core/protocol/message_result.hpp"
#include "wirekrak/core/protocol/kraken/enums.hpp"
#include "wirekrak/core/protocol/kraken/parser/router.hpp"


namespace wirekrak::core::protocol::kraken {

class MessageHandler {
public:

    MessageHandler() = default;

    // =========================================================================
    // Entry point (HandlerConcept)
    // =========================================================================
    template<class Context>
    [[nodiscard]]
    inline MessageResult on_message(Context& ctx, std::string_view msg) noexcept {

        // Fast-path: empty
        if (msg.empty()) [[unlikely]] {
            return MessageResult::Ignored;
        }

        Method method{};
        Channel channel{};

        return router_.parse_and_route(ctx, msg, method, channel);
    }

private:
    parser::Router router_;
};

} // namespace wirekrak::core::protocol::kraken

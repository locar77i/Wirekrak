#pragma once


namespace wirekrak::protocol::kraken::response {

/*
===============================================================================
Response Traits (Core Protocol Extension Point)
===============================================================================

traits<ResponseT> defines how a protocol-level Response is decomposed
into symbol-scoped views suitable for deterministic routing and dispatch.

Each specialization describes:
  - The concrete response type (ResponseT)
  - The message element type contained in the response
  - The corresponding ResponseView type
  - How to extract symbols from individual messages
  - How to access the message collection in the response
  - How to construct a symbol-scoped ResponseView

Design intent:
  - Separate protocol schema definitions from response interpretation logic
  - Provide a compile-time extension point for new Kraken channels
  - Enable generic, reusable infrastructure (e.g. Classifier)
  - Avoid runtime polymorphism, hooks, or type erasure

Usage rules:
  - The primary template is intentionally undefined
  - Every supported Response type MUST provide a specialization
  - Specializations must be stateless, side-effect free, and fully inlineable
  - No memory allocation or ownership is permitted in trait functions

Architectural role:
  - traits is part of Wirekrak Core infrastructure
  - It is not user-facing and not intended for Lite-level consumption
  - It encodes protocol invariants and projection rules

Adding a new channel:
  - Define the protocol schema (schema::*)
  - Define the corresponding ResponseView
  - Provide a traits specialization for the Response type

===============================================================================
*/

// Primary template (undefined on purpose)
template<class ResponseT>
struct traits;

} // namespace wirekrak::protocol::kraken::response


#include "wirekrak/protocol/kraken/schema/trade/response.hpp"
#include "wirekrak/protocol/kraken/schema/trade/response_view.hpp"

namespace wirekrak::protocol::kraken::response {

template<>
struct traits<schema::trade::Response> {
    using response_type = schema::trade::Response;
    using message_type  = schema::trade::Trade;
    using view_type     = schema::trade::ResponseView;

    static inline Symbol symbol_of(const message_type& msg) noexcept {
        return msg.symbol;
    }

    static inline view_type make_view(Symbol symbol, PayloadType type, std::span<const message_type* const> msgs) noexcept {
        return view_type{
            .symbol = symbol,
            .type   = type,
            .trades = msgs
        };
    }

    static inline const std::vector<message_type>& messages(const response_type& resp) noexcept {
        return resp.trades;
    }

    static inline PayloadType payload_type(const response_type& resp) noexcept {
        return resp.type;
    }
};

} // namespace wirekrak::protocol::kraken::response
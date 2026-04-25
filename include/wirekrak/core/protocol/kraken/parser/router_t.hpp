#pragma once

#include <string>


#include <simdjson.h>

#include "wirekrak/core/protocol/message_result.hpp"
#include "wirekrak/core/protocol/kraken/enums.hpp"
#include "wirekrak/core/protocol/kraken/context.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/adapters.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/rejection_notice.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/status/update.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/system/pong.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/trade/subscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/trade/response.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/trade/unsubscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/book/subscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/book/response.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/book/unsubscribe_ack.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace parser {

/*
================================================================================
Kraken WebSocket Parsing Architecture
================================================================================

This parser layer is intentionally structured into three distinct roles to
ensure correctness, performance, and long-term maintainability.

-------------------------------------------------------------------------------
1) Parser RouterT (Message Dispatch)
-------------------------------------------------------------------------------
The parser router is responsible for:
  • Inspecting raw WebSocket messages
  • Routing messages by method / channel
  • Selecting the appropriate message parser
  • Enforcing high-level protocol flow

The router performs no field-level parsing and contains no domain logic.
It exists solely to orchestrate message dispatch safely and efficiently.

-------------------------------------------------------------------------------
2) Message Parsers (Protocol-Level Validation)
-------------------------------------------------------------------------------
Message parsers implement full Kraken message schemas (subscribe ACKs,
updates, snapshots, control messages, rejections, etc.).

Responsibilities:
  • Validate required vs optional fields
  • Apply protocol rules (success vs error paths)
  • Log parsing failures with actionable diagnostics
  • Populate strongly-typed domain structures

Message parsers are allowed to:
  • Reject malformed or semantically invalid messages
  • Decide whether a message should be ignored or propagated
  • Perform control-flow decisions

They are NOT responsible for low-level JSON extraction.

-------------------------------------------------------------------------------
3) Adapters (Domain-Aware Field Parsing)
-------------------------------------------------------------------------------
Adapters sit between message parsers and low-level helpers.

Responsibilities:
  • Convert primitive fields into domain types (Symbol, Side, OrderType, etc.)
  • Enforce semantic validity (non-empty strings, known enums, valid ranges)
  • Distinguish between invalid schema and invalid values
  • Remain allocation-light and exception-free

Adapters are domain-aware but schema-agnostic.

-------------------------------------------------------------------------------
4) Helpers (Low-Level JSON Primitives)
-------------------------------------------------------------------------------
Helpers are the lowest-level building blocks and are intentionally strict.

Responsibilities:
  • Enforce JSON structural correctness (object, array, type)
  • Parse primitive types without allocation
  • Provide explicit optional-field presence signaling
  • Never perform semantic or domain validation
  • Never log, throw, or allocate

Helpers return a lightweight MessageResult enum to distinguish:
  • Ok              → structurally valid
  • InvalidSchema   → malformed JSON or wrong types
  • InvalidValue    → reserved for higher layers

-------------------------------------------------------------------------------
Design Goals
-------------------------------------------------------------------------------
  • Zero runtime overhead abstractions
  • Clear separation of responsibilities
  • Deterministic, testable parsing behavior
  • Robust handling of real-world Kraken API inconsistencies
  • Compile-time safety where possible, runtime safety everywhere else

This layered design allows each component to remain simple, focused, and
correct, while enabling the overall system to scale as Kraken schemas evolve.

================================================================================
*/

class RouterT {

    constexpr static size_t PARSER_BUFFER_INITIAL_SIZE_ = 16 * 1024; // 16 KB

public:
    RouterT() = default;

    // Main entry point
    template<class Context>
    [[nodiscard]]
    inline MessageResult parse_and_route(Context& ctx, std::string_view raw_msg, Method& method, Channel& channel) noexcept {
        using namespace simdjson;
        // Parse JSON message
        simdjson::dom::element root;
        auto error = parser_.parse(raw_msg).get(root);
        if (error) {
            WK_WARN("[PARSER] JSON parse error: " << error << " in message: " << raw_msg);
            return MessageResult::InvalidSchema;
        }
        // METHOD DISPATCH (ACK / CONTROL)
        if (dom::adapter::parse_method_required(root, method) == MessageResult::Parsed) {
            return parse_method_message_(ctx, method, root);
        }
        // CHANNEL DISPATCH (DATA)
        if (dom::adapter::parse_channel_required(root, channel) == MessageResult::Parsed) {
            return parse_channel_message_(ctx, channel, root);
        }
        return MessageResult::Ignored;
    }


private:
    // Underlying simdjson parser
    simdjson::dom::parser parser_;

private:

    // =========================================================================
    // Parse helpers for method messages
    // =========================================================================

    template<class Context>
    [[nodiscard]]
    inline MessageResult parse_method_message_(Context& ctx, Method method, const simdjson::dom::element& root) noexcept {
        using namespace simdjson;

        // Fix 1nd kraken API inconsistency: 'result' object is not present in 'pong' messages ...
        // ------------------------------------------------------------------------
        // Control-scoped messages:
        // - Do NOT require result
        // - Do NOT require channel
        // ------------------------------------------------------------------------
        switch (method) {
            case Method::Pong:
                return parse_pong_(ctx, root);
            default:
                break;
        }
        
        // ------------------------------------------------------------------------
        // Channel-scoped messages:
        // - Require result
        // - Require channel
        // ------------------------------------------------------------------------

        // Fix 2nd kraken API inconsistency: Kraken omits the 'result' object on failed subscribe/unsubscribe responses.
        // On success == false, only 'error' is guaranteed to be present.
        Channel channel;
        // result object (required)
        simdjson::dom::element result;
        if (dom::helper::parse_object_required(root, "result", result) != MessageResult::Parsed) {
            channel = Channel::Unknown;
        }
        else {
            // channel (required)
            if (dom::adapter::parse_channel_required(result, channel) != MessageResult::Parsed) {
                WK_WARN("[PARSER] Field 'channel' missing or invalid in '" << to_string(method) << "' message -> ignore message.");
                channel = Channel::Unknown;
            }
        }
        switch (method) {
            case Method::Subscribe:
                return parse_subscribe_ack(ctx, channel, root);
            case Method::Unsubscribe:
                return parse_unsubscribe_ack(ctx, channel, root);
            default:
                WK_WARN("[PARSER] Unhandled method -> ignore");
        }
        return MessageResult::Ignored;
    }

    // SUBSCRIBE ACK PARSER
    template<class Context>
    [[nodiscard]]
    inline MessageResult parse_subscribe_ack(Context& ctx, Channel channel, const simdjson::dom::element& root) noexcept {
        auto r = MessageResult::Ignored;
        switch (channel) {
            case Channel::Trade: {
                schema::trade::SubscribeAck resp;
                r = dom::trade::subscribe_ack::parse(root, resp);
                if (r == MessageResult::Parsed) {
                    ctx.template on_subscribe_ack<schema::trade::Subscribe>(
                        resp.req_id.value(),
                        resp.symbol,
                        resp.success
                    );
                    return MessageResult::Delivered;
                }
                WK_WARN("[PARSER] Failed to parse trade subscribe ACK.");
            } break;
            case Channel::Book: {
                schema::book::SubscribeAck resp;
                r = dom::book::subscribe_ack::parse(root, resp);
                if (r == MessageResult::Parsed) {
                    ctx.template on_subscribe_ack<schema::book::Subscribe>(
                        resp.req_id.value(),
                        resp.symbol,
                        resp.success
                    );
                    return MessageResult::Delivered;
                }
                WK_WARN("[PARSER] Failed to parse book subscribe ACK.");
            } break;
            default: { // 2025-12-20 08:39:28 [WARN] [PARSER] Failed to parse method message: {"error":"Already subscribed","method":"subscribe","req_id":2,"success":false,"symbol":"BTC/USD","time_in":"2025-12-20T07:39:28.809188Z","time_out":"2025-12-20T07:39:28.809200Z"}
                schema::rejection::Notice resp;
                r = dom::rejection_notice::parse(root, resp);
                if (r == MessageResult::Parsed) {
                    WK_WARN("[SESSION] Handling rejection notice for symbol {" << (resp.symbol.has() ? resp.symbol.value() : Symbol("N/A") )
                        << "} (req_id=" << (resp.req_id.has() ? resp.req_id.value() : ctrl::INVALID_REQ_ID) << ") - " << resp.error);
                    // 1. CONTROL PLANE (internal correctness)
                    if (resp.req_id.has() && resp.symbol.has()) {
                        ctx.on_rejection(resp.req_id.value(), resp.symbol.value());
                    }
                    // 2. DATA PLANE (user visibility)
                    if (!ctx.push(std::move(resp))) {
                        return MessageResult::Backpressure;
                    }
                    return MessageResult::Delivered;
                }
                WK_WARN("[PARSER] Failed to parse rejection notice.");
            } break;
        }
        return r;
    }

    // UNSUBSCRIBE ACK PARSER
    template<class Context>
    [[nodiscard]]
    inline MessageResult parse_unsubscribe_ack(Context& ctx, Channel channel, const simdjson::dom::element& root) noexcept {
        auto r = MessageResult::Ignored;
        switch (channel) {
            case Channel::Trade: {
                schema::trade::UnsubscribeAck resp;
                r = dom::trade::unsubscribe_ack::parse(root, resp);
                if (r == MessageResult::Parsed) {
                    ctx.template on_unsubscribe_ack<schema::trade::Subscribe>(
                        resp.req_id.value(),
                        resp.symbol,
                        resp.success
                    );
                    return MessageResult::Delivered;
                }
                WK_WARN("[PARSER] Failed to parse trade unsubscribe ACK.");
            } break;
            case Channel::Book: {
                schema::book::UnsubscribeAck resp;
                r = dom::book::unsubscribe_ack::parse(root, resp);
                if (r == MessageResult::Parsed) {
                    ctx.template on_unsubscribe_ack<schema::book::Subscribe>(
                        resp.req_id.value(),
                        resp.symbol,
                        resp.success
                    );
                    return MessageResult::Delivered;
                }
                WK_WARN("[PARSER] Failed to parse book unsubscribe ACK.");
            } break;
            default: { // 2025-12-20 08:39:43 [WARN] [PARSER] Failed to parse method message: {"error":"Subscription Not Found","method":"subscribe","req_id":4,"success":false,"symbol":"BTC/USD","time_in":"2025-12-20T07:39:43.909056Z","time_out":"2025-12-20T07:39:43.909073Z"}
                schema::rejection::Notice resp;
                r = dom::rejection_notice::parse(root, resp);
                if (r == MessageResult::Parsed) {
                    WK_WARN("[SESSION] Handling rejection notice for symbol {" << (resp.symbol.has() ? resp.symbol.value() : Symbol("N/A") )
                        << "} (req_id=" << (resp.req_id.has() ? resp.req_id.value() : ctrl::INVALID_REQ_ID) << ") - " << resp.error);
                    // 1. CONTROL PLANE (internal correctness)
                    if (resp.req_id.has() && resp.symbol.has()) {
                        ctx.on_rejection(resp.req_id.value(), resp.symbol.value());
                    }
                    // 2. DATA PLANE (user visibility)
                    if (!ctx.push(std::move(resp))) {
                        return MessageResult::Backpressure;
                    }
                    return MessageResult::Delivered;
                }
                WK_WARN("[PARSER] Failed to parse rejection notice.");
            } break;
        }
        return r;
    }


    // ========================================================================
    // Parse helpers for channel messages
    // ========================================================================

    template<class Context>
    [[nodiscard]]
    inline MessageResult parse_channel_message_(Context& ctx, Channel channel, const simdjson::dom::element& root) noexcept {
        switch (channel) {
            case Channel::Trade:
                return parse_trade_(ctx, root);
            case Channel::Ticker:
                return parse_ticker_(ctx, root);
            case Channel::Book:
                return parse_book_(ctx, root);
            case Channel::Heartbeat:
                // TODO: parse heartbeat message and update heartbeat stats in context
                return MessageResult::Delivered;
            case Channel::Status: {
                return parse_status_(ctx, root);
            } break;
            default:
                WK_WARN("[PARSER] Unhandled channel -> ignore");
                break;
        }
        return MessageResult::Ignored;
    }

    // TRADE PARSER
    template<class Context>
    [[nodiscard]]
    inline MessageResult parse_trade_(Context& ctx, const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
        schema::trade::Response response;
        auto r = dom::trade::response::parse(root, response);
        if (r == MessageResult::Parsed) {
            if (!ctx.push(std::move(response))) {
                return MessageResult::Backpressure;
            }
            return MessageResult::Delivered;
        }
        return r;
    }

    // TICKER PARSER
    template<class Context>
    [[nodiscard]]
    inline MessageResult parse_ticker_(Context& ctx, const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
        WK_WARN("[PARSER] Unhandled channel 'ticker' -> ignore");
        // TODO
        return MessageResult::Ignored;
    }

    // BOOK PARSER
    template<class Context>
    [[nodiscard]]
    inline MessageResult parse_book_(Context& ctx, const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
        schema::book::Response response;
        auto r = dom::book::response::parse(root, response);
        if (r == MessageResult::Parsed) {
            if (!ctx.push(std::move(response))) {
                return MessageResult::Backpressure;
            }
            return MessageResult::Delivered;
        }
        return r;
    }

    // PONG PARSER
    template<class Context>
    [[nodiscard]]
    inline MessageResult parse_pong_(Context& ctx, const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
        schema::system::Pong resp;
        auto r = dom::system::pong::parse(root, resp);
        if (r == MessageResult::Parsed) {
            // We intentionally overwrite the previous value: no backpressure, no queuing, freshness over history
            ctx.set(std::move(resp));
            return MessageResult::Delivered;
        }
        return r;
    }

    template<class Context>
    [[nodiscard]]
    inline MessageResult parse_status_(Context& ctx, const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
        schema::status::Update resp;
        auto r = dom::status::update::parse(root, resp);
        if (r == MessageResult::Parsed) {
            // We intentionally overwrite the previous value: no backpressure, no queuing, freshness over history
            ctx.set(std::move(resp));
            return MessageResult::Delivered;
        }
        return r;
    }

};

} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core

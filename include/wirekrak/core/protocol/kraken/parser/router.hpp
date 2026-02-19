#pragma once

#include <string>
#include <atomic>
#include <iostream>

#include <simdjson.h>

#include "wirekrak/core/protocol/kraken/enums.hpp"
#include "wirekrak/core/protocol/kraken/context.hpp"
#include "wirekrak/core/protocol/kraken/parser/adapters.hpp"
#include "wirekrak/core/protocol/kraken/parser/rejection_notice.hpp"
#include "wirekrak/core/protocol/kraken/parser/status/update.hpp"
#include "wirekrak/core/protocol/kraken/parser/system/pong.hpp"
#include "wirekrak/core/protocol/kraken/parser/trade/subscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/parser/trade/response.hpp"
#include "wirekrak/core/protocol/kraken/parser/trade/unsubscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/parser/book/subscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/parser/book/response.hpp"
#include "wirekrak/core/protocol/kraken/parser/book/unsubscribe_ack.hpp"
#include "lcr/log/logger.hpp"
#include "lcr/lockfree/spsc_ring.hpp"


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
1) Parser Router (Message Dispatch)
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

Helpers return a lightweight Result enum to distinguish:
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


class Router {
    constexpr static size_t PARSER_BUFFER_INITIAL_SIZE_ = 16 * 1024; // 16 KB

public:
    Router(ContextView& ctx)
        : ctx_view_(ctx)
    {
    }


    // Main entry point
    [[nodiscard]]
    inline Result parse_and_route(std::string_view raw_msg) noexcept {
        using namespace simdjson;
        // Parse JSON message
        simdjson::dom::element root;
        auto error = parser_.parse(raw_msg).get(root);
        if (error) {
            WK_WARN("[PARSER] JSON parse error: " << error << " in message: " << raw_msg);
            return Result::InvalidSchema;
        }
        // METHOD DISPATCH (ACK / CONTROL)
        Method method;
        if (adapter::parse_method_required(root, method) == Result::Parsed) {
            return parse_method_message_(method, root);
        }
        // CHANNEL DISPATCH (DATA)
        Channel channel;
        if (adapter::parse_channel_required(root, channel) == Result::Parsed) {
            return parse_channel_message_(channel, root);
        }
        return Result::Ignored;
    }


private:
    // Context view (non-owning)
    ContextView& ctx_view_;

    // Underlying simdjson parser
    simdjson::dom::parser parser_;

private:

    // =========================================================================
    // Parse helpers for method messages
    // =========================================================================

    [[nodiscard]]
    inline Result parse_method_message_(Method method, const simdjson::dom::element& root) noexcept {
        using namespace simdjson;

        // Fix 1nd kraken API inconsistency: 'result' object is not present in 'pong' messages ...
        // ------------------------------------------------------------------------
        // Control-scoped messages:
        // - Do NOT require result
        // - Do NOT require channel
        // ------------------------------------------------------------------------
        switch (method) {
            case Method::Pong:
                return parse_pong_(root);
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
        if (helper::parse_object_required(root, "result", result) != Result::Parsed) {
            channel = Channel::Unknown;
        }
        else {
            // channel (required)
            if (adapter::parse_channel_required(result, channel) != Result::Parsed) {
                WK_WARN("[PARSER] Field 'channel' missing or invalid in '" << to_string(method) << "' message -> ignore message.");
                channel = Channel::Unknown;
            }
        }
        switch (method) {
            case Method::Subscribe:
                return parse_subscribe_ack(channel, root);
            case Method::Unsubscribe:
                return parse_unsubscribe_ack(channel, root);
            default:
                WK_WARN("[PARSER] Unhandled method -> ignore");
        }
        return Result::Ignored;
    }

    // SUBSCRIBE ACK PARSER
    [[nodiscard]]
    inline Result parse_subscribe_ack(Channel channel, const simdjson::dom::element& root) noexcept {
        auto r = Result::Ignored;
        switch (channel) {
            case Channel::Trade: {
                schema::trade::SubscribeAck resp;
                r = trade::subscribe_ack::parse(root, resp);
                if (r == Result::Parsed) {
                    if (!ctx_view_.trade_subscribe_ring.push(std::move(resp))) { // TODO: handle backpressure
                        WK_WARN("[PARSER] Trade subscribe ring full - message has not been delivered.");
                        return Result::Backpressure;
                    }
                    return Result::Delivered;
                }
                WK_WARN("[PARSER] Failed to parse trade subscribe ACK.");
            } break;
            case Channel::Book: {
                schema::book::SubscribeAck resp;
                r = book::subscribe_ack::parse(root, resp);
                if (r == Result::Parsed) {
                    if (!ctx_view_.book_subscribe_ring.push(std::move(resp))) { // TODO: handle backpressure
                        WK_WARN("[PARSER] Book subscribe ring full - message has not been delivered.");
                        return Result::Backpressure;
                    }
                    return Result::Delivered;
                }
                WK_WARN("[PARSER] Failed to parse book subscribe ACK.");
            } break;
            default: { // 2025-12-20 08:39:28 [WARN] [PARSER] Failed to parse method message: {"error":"Already subscribed","method":"subscribe","req_id":2,"success":false,"symbol":"BTC/USD","time_in":"2025-12-20T07:39:28.809188Z","time_out":"2025-12-20T07:39:28.809200Z"}
                schema::rejection::Notice resp;
                r = rejection_notice::parse(root, resp);
                if (r == Result::Parsed) {
                    if (!ctx_view_.rejection_ring.push(std::move(resp))) { // TODO: handle backpressure
                        WK_WARN("[PARSER] Rejection ring full - message has not been delivered.");
                        return Result::Backpressure;
                    }
                    return Result::Delivered;
                }
                WK_WARN("[PARSER] Failed to parse rejection notice.");
            } break;
        }
        return r;
    }

    // UNSUBSCRIBE ACK PARSER
    [[nodiscard]]
    inline Result parse_unsubscribe_ack(Channel channel, const simdjson::dom::element& root) noexcept {
        auto r = Result::Ignored;
        switch (channel) {
            case Channel::Trade: {
                schema::trade::UnsubscribeAck resp;
                r = trade::unsubscribe_ack::parse(root, resp);
                if (r == Result::Parsed) {
                    if (!ctx_view_.trade_unsubscribe_ring.push(std::move(resp))) { // TODO: handle backpressure
                        WK_WARN("[PARSER] Trade unsubscribe ring full - message has not been delivered.");
                        return Result::Backpressure;
                    }
                    return Result::Delivered;
                }
                WK_WARN("[PARSER] Failed to parse trade unsubscribe ACK.");
            } break;
            case Channel::Book: {
                schema::book::UnsubscribeAck resp;
                r = book::unsubscribe_ack::parse(root, resp);
                if (r == Result::Parsed) {
                    if (!ctx_view_.book_unsubscribe_ring.push(std::move(resp))) { // TODO: handle backpressure
                        WK_WARN("[PARSER] Book unsubscribe ring full - message has not been delivered.");
                        return Result::Backpressure;
                    }
                    return Result::Delivered;
                }
                WK_WARN("[PARSER] Failed to parse book unsubscribe ACK.");
            } break;
            default: { // 2025-12-20 08:39:43 [WARN] [PARSER] Failed to parse method message: {"error":"Subscription Not Found","method":"subscribe","req_id":4,"success":false,"symbol":"BTC/USD","time_in":"2025-12-20T07:39:43.909056Z","time_out":"2025-12-20T07:39:43.909073Z"}
                schema::rejection::Notice resp;
                r = rejection_notice::parse(root, resp);
                if (r == Result::Parsed) {
                    if (!ctx_view_.rejection_ring.push(std::move(resp))) { // TODO: handle backpressure
                        WK_WARN("[PARSER] Rejection ring full - message has not been delivered.");
                        return Result::Backpressure;
                    }
                    return Result::Delivered;
                }
                WK_WARN("[PARSER] Failed to parse rejection notice.");
            } break;
        }
        return r;
    }


    // ========================================================================
    // Parse helpers for channel messages
    // ========================================================================

    [[nodiscard]]
    inline Result parse_channel_message_(Channel channel, const simdjson::dom::element& root) noexcept {
        switch (channel) {
            case Channel::Trade:
                return parse_trade_(root);
            case Channel::Ticker:
                return parse_ticker_(root);
            case Channel::Book:
                return parse_book_(root);
            case Channel::Heartbeat:
                ctx_view_.heartbeat_total.fetch_add(1, std::memory_order_relaxed);
                ctx_view_.last_heartbeat_ts.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
                return Result::Delivered;
            case Channel::Status: {
                return parse_status_(root);
            } break;
            default:
                WK_WARN("[PARSER] Unhandled channel -> ignore");
                break;
        }
        return Result::Ignored;
    }

    // TRADE PARSER
    [[nodiscard]]
    inline Result parse_trade_(const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
        schema::trade::Response response;
        auto r = trade::response::parse(root, response);
        if (r == Result::Parsed) {
            if (!ctx_view_.trade_ring.push(std::move(response))) {
                return Result::Backpressure;
            }
            return Result::Delivered;
        }
        return r;
    }

    // TICKER PARSER
    [[nodiscard]]
    inline Result parse_ticker_(const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
        WK_WARN("[PARSER] Unhandled channel 'ticker' -> ignore");
        // TODO
        return Result::Ignored;
    }

    // BOOK PARSER
    [[nodiscard]]
    inline Result parse_book_(const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
        schema::book::Response response;
        auto r = book::response::parse(root, response);
        if (r == Result::Parsed) {
            if (!ctx_view_.book_ring.push(std::move(response))) {
                return Result::Backpressure;
            }
            return Result::Delivered;
        }
        return r;
    }

    // PONG PARSER
    [[nodiscard]]
    inline Result parse_pong_(const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
        schema::system::Pong resp;
        auto r = system::pong::parse(root, resp);
        if (r == Result::Parsed) {
            // We intentionally overwrite the previous value: no backpressure, no queuing, freshness over history
            ctx_view_.pong_slot.store(std::move(resp));
            return Result::Delivered;
        }
        return r;
    }

    [[nodiscard]]
    inline Result parse_status_(const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
        schema::status::Update resp;
        auto r = status::update::parse(root, resp);
        if (r == Result::Parsed) {
            // We intentionally overwrite the previous value: no backpressure, no queuing, freshness over history
            ctx_view_.status_slot.store(std::move(resp));
            return Result::Delivered;
        }
        return r;
    }

};

} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core

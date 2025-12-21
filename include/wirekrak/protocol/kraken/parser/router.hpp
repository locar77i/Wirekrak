#pragma once

#include <string>
#include <atomic>
#include <iostream>

#include <simdjson.h>

#include "wirekrak/protocol/kraken/enums.hpp"
#include "wirekrak/protocol/kraken/parser/context.hpp"
#include "wirekrak/protocol/kraken/parser/adapters.hpp"
#include "wirekrak/protocol/kraken/parser/rejection_notice.hpp"
#include "wirekrak/protocol/kraken/parser/status/update.hpp"
#include "wirekrak/protocol/kraken/parser/system/pong.hpp"
#include "wirekrak/protocol/kraken/parser/trade/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/parser/trade/response.hpp"
#include "wirekrak/protocol/kraken/parser/trade/unsubscribe_ack.hpp"
#include "wirekrak/protocol/kraken/parser/book/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/parser/book/snapshot.hpp"
#include "wirekrak/protocol/kraken/parser/book/update.hpp"
#include "wirekrak/protocol/kraken/parser/book/unsubscribe_ack.hpp"
#include "lcr/log/logger.hpp"
#include "lcr/lockfree/spsc_ring.hpp"


namespace wirekrak {
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

Helpers return a lightweight parser::Result enum to distinguish:
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
    Router(const Context& ctx)
        : ctx_(ctx)
    {
    }


    inline void parse_and_route(const std::string& raw_msg) noexcept {
        using namespace simdjson;
        simdjson::dom::element root;
        // Parse JSON message
        auto error = parser_.parse(raw_msg).get(root);
        if (error) {
            WK_WARN("[PARSER] JSON parse error: " << error << " in message: " << raw_msg);
            return;
        }
        // METHOD DISPATCH (ACK / CONTROL)
        Method method;
        auto r = adapter::parse_method_required(root, method);
        if (r == parser::Result::Ok) {
            if (!parse_method_message_(method, root)) {
                WK_WARN("[PARSER] Failed to parse method message: " << raw_msg);
            }
            return; // method messages never fall through
        }
        // CHANNEL DISPATCH (DATA)
        Channel channel;
        r = adapter::parse_channel_required(root, channel);
        if (r == parser::Result::Ok) {
            if (!parse_channel_message_(channel, root)) {
                WK_WARN("[PARSER] Failed to parse channel message: " << raw_msg);
            }
            return;
        }
    }


private:
    Context ctx_;
    simdjson::dom::parser parser_;

private:

    // =========================================================================
    // Parse helpers for method messages
    // =========================================================================

    [[nodiscard]] inline bool parse_method_message_(Method method, const simdjson::dom::element& root) noexcept {
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
        auto r = helper::parse_object_required(root, "result", result);
        if (r != parser::Result::Ok) {
            channel = Channel::Unknown;
        }
        else {
            // channel (required)
            r = adapter::parse_channel_required(result, channel);
            if (r != parser::Result::Ok) {
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
        return false;
    }

    // SUBSCRIBE ACK PARSER
    [[nodiscard]] inline bool parse_subscribe_ack(Channel channel, const simdjson::dom::element& root) noexcept {
        switch (channel) {
            case Channel::Trade: {
                kraken::trade::SubscribeAck resp;
                if (trade::subscribe_ack::parse(root, resp)) {
                    if (!ctx_.trade_subscribe_ring->push(std::move(resp))) { // TODO: handle backpressure
                        WK_WARN("[PARSER] trade_subscribe_ring_ full, dropping.");
                    }
                    return true;
                }
                WK_WARN("[PARSER] Failed to parse trade subscribe ACK.");
            } break;
            case Channel::Book: {
                kraken::book::SubscribeAck resp;
                if (book::subscribe_ack::parse(root, resp)) {
                    if (!ctx_.book_subscribe_ring->push(std::move(resp))) { // TODO: handle backpressure
                        WK_WARN("[PARSER] book_subscribe_ring_ full, dropping.");
                    }
                    return true;
                }
                WK_WARN("[PARSER] Failed to parse book subscribe ACK.");
            } break;
            default: { // 2025-12-20 08:39:28 [WARN] [PARSER] Failed to parse method message: {"error":"Already subscribed","method":"subscribe","req_id":2,"success":false,"symbol":"BTC/USD","time_in":"2025-12-20T07:39:28.809188Z","time_out":"2025-12-20T07:39:28.809200Z"}
                kraken::rejection::Notice resp;
                if (rejection_notice::parse(root, resp)) {
                    if (!ctx_.rejection_ring->push(std::move(resp))) { // TODO: handle backpressure
                        WK_WARN("[PARSER] rejection_ring_ full, dropping.");
                    }
                    return true;
                }
                WK_WARN("[PARSER] Failed to parse rejection notice.");
            } break;
        }
        return false;
    }

    // UNSUBSCRIBE ACK PARSER
    [[nodiscard]] inline bool parse_unsubscribe_ack(Channel channel, const simdjson::dom::element& root) noexcept {
        switch (channel) {
            case Channel::Trade: {
                kraken::trade::UnsubscribeAck resp;
                if (trade::unsubscribe_ack::parse(root, resp)) {
                    if (!ctx_.trade_unsubscribe_ring->push(std::move(resp))) { // TODO: handle backpressure
                        WK_WARN("[PARSER] trade_unsubscribe_ring_ full, dropping.");
                    }
                    return true;
                }
            } break;
            case Channel::Book: {
                kraken::book::UnsubscribeAck resp;
                if (book::unsubscribe_ack::parse(root, resp)) {
                    if (!ctx_.book_unsubscribe_ring->push(std::move(resp))) { // TODO: handle backpressure
                        WK_WARN("[PARSER] book_unsubscribe_ring_ full, dropping.");
                    }
                    return true;
                }
            } break;
            default: { // 2025-12-20 08:39:43 [WARN] [PARSER] Failed to parse method message: {"error":"Subscription Not Found","method":"subscribe","req_id":4,"success":false,"symbol":"BTC/USD","time_in":"2025-12-20T07:39:43.909056Z","time_out":"2025-12-20T07:39:43.909073Z"}
                kraken::rejection::Notice resp;
                if (rejection_notice::parse(root, resp)) {
                    if (!ctx_.rejection_ring->push(std::move(resp))) { // TODO: handle backpressure
                        WK_WARN("[PARSER] rejection_ring_ full, dropping.");
                    }
                    return true;
                }
                WK_WARN("[PARSER] Failed to parse rejection notice.");
            } break;
        }
        return false;
    }


    // ========================================================================
    // Parse helpers for channel messages
    // ========================================================================

    [[nodiscard]] inline bool parse_channel_message_(Channel channel, const simdjson::dom::element& root) noexcept {
        switch (channel) {
            case Channel::Trade:
                return parse_trade_(root);
            case Channel::Ticker:
                return parse_ticker_(root);;
            case Channel::Book:
                return parse_book_(root);
            case Channel::Heartbeat:
                ctx_.heartbeat_total->fetch_add(1, std::memory_order_relaxed);
                ctx_.last_heartbeat_ts->store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
                return true;
            case Channel::Status: {
                return parse_status_(root);
            } break;
            default:
                WK_WARN("[PARSER] Unhandled channel -> ignore");
                break;
        }
        return false;
    }

    // TRADE PARSER
    [[nodiscard]] inline bool parse_trade_(const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
         kraken::trade::Response response;
        if (trade::response::parse(root, response)) {
            if (!ctx_.trade_ring->push(std::move(response))) { // TODO: handle backpressure
                WK_WARN("[PARSER] trade_ring_ full, dropping.");
            }
            return true;
        }
        return false;
    }

    // TICKER PARSER
    [[nodiscard]] inline bool parse_ticker_(const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
        WK_WARN("[PARSER] Unhandled channel 'ticker' -> ignore");
        // TODO
        return false;
    }

    // BOOK PARSER
    [[nodiscard]] inline bool parse_book_(const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
        // type field
        auto type = root["type"].get_string();
        if (type.error()) {
            WK_WARN("[PARSER] book message missing type -> ignore");
            return false;
        }
        // route based on type
        switch (to_payload_type_enum_fast(type.value())) {
            case PayloadType::Snapshot: {
                kraken::book::Snapshot snapshot;
                if (book::snapshot::parse(root, snapshot)) {
                    // push snapshot (ring / callback / reducer)
                    return true;
                }
            } break;
            case PayloadType::Update: {
                kraken::book::Update update;
                if (book::update::parse(root, update)) {
                    if (!ctx_.book_ring->push(std::move(update))) { // TODO: handle backpressure
                        WK_WARN("[PARSER] book_ring_ full, dropping.");
                    }
                    return true;
                }
            } break;
            default:
                WK_WARN("[PARSER] Unknown book type -> ignore");
                break;
        }

        return false;
    }

    // PONG PARSER
    [[nodiscard]] inline bool parse_pong_(const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
        kraken::system::Pong resp;
        if (system::pong::parse(root, resp)) {
            if (!ctx_.pong_ring->push(std::move(resp))) { // TODO: handle backpressure
                WK_WARN("[PARSER] pong_ring_ full, dropping.");
            }
            return true;
        }
        return false;
    }

    [[nodiscard]] inline bool parse_status_(const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
        kraken::status::Update resp;
        if (status::update::parse(root, resp)) {
             if (!ctx_.status_ring->push(std::move(resp))) { // TODO: handle backpressure
                WK_WARN("[PARSER] status_ring_ full, dropping.");
            }
            return true;
        }
        return false;
    }

};

} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

#pragma once

#include <string>
#include <atomic>
#include <iostream>

#include <simdjson.h>

#include "wirekrak/protocol/kraken/enums.hpp"
#include "wirekrak/protocol/kraken/trade/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/trade/response.hpp"
#include "wirekrak/protocol/kraken/trade/unsubscribe_ack.hpp"
#include "wirekrak/protocol/kraken/parser/context.hpp"
#include "wirekrak/protocol/kraken/parser/status/update.hpp"
#include "wirekrak/protocol/kraken/parser/trade/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/parser/trade/response.hpp"
#include "wirekrak/protocol/kraken/parser/trade/unsubscribe_ack.hpp"
#include "wirekrak/protocol/kraken/parser/book/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/parser/book/snapshot.hpp"
#include "wirekrak/protocol/kraken/parser/book/update.hpp"
#include "wirekrak/protocol/kraken/parser/book/unsubscribe_ack.hpp"
#include "wirekrak/core/symbol.hpp"
#include "lcr/log/logger.hpp"
#include "lcr/lockfree/spsc_ring.hpp"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace parser {

    // Payload type enum
    enum class PayloadType : uint8_t {
        Snapshot,
        Update,
        Unknown
    };
    // Determine payload type from string
    [[nodiscard]] inline constexpr PayloadType to_payload_type(std::string_view s) noexcept {
        if (s == "snapshot") return PayloadType::Snapshot;
        if (s == "update")   return PayloadType::Update;
        return PayloadType::Unknown;
    }


class Router {
    constexpr static size_t PARSER_BUFFER_INITIAL_SIZE_ = 16 * 1024; // 16 KB

public:
    Router(const Context& ctx)
        : ctx_(ctx)
    {
    }


    inline void parse_and_route(const std::string& raw_msg) noexcept {
        using namespace simdjson;
        dom::element root;
        // Parse JSON message
        auto error = parser_.parse(raw_msg).get(root);
        if (error) {
            WK_WARN("[PARSER] JSON parse error: " << error << " in message: " << raw_msg);
            return;
        }
        // 1) METHOD DISPATCH
        auto method_field = root["method"];
        if (!method_field.error()) {
            std::string_view method_sv;
            if (!method_field.get(method_sv)) {
                Method method = to_method_enum_fast(method_sv);
                if (method != Method::Unknown) {
                    if (!parse_method_message_(method, root)) {
                        WK_WARN("[PARSER] Failed to parse method message: " << raw_msg);
                    }
                }
                else {
                    WK_WARN("[PARSER] Unknown method: " << method_sv);
                }
            }
            else {
                WK_WARN("[PARSER] 'method' is not a string");
            }
            return; // ACK/NACK messages do not go into rings, they are just control messages
        }
        // 2) CHANNEL DISPATCH
        auto channel_field = root["channel"];
        if (!channel_field.error()) { // channel exists
            std::string_view channel_sv;
            if (!channel_field.get(channel_sv)) {
                Channel channel = to_channel_enum_fast(channel_sv);
                if (channel != Channel::Unknown) {
                    if (!parse_channel_message_(channel, root)) {
                        WK_WARN("[PARSER] Failed to parse channel message: " << raw_msg);
                    }
                }
                else {
                    WK_WARN("[PARSER] Unknown channel: " << channel_sv);
                }
            }
            else {
                WK_WARN("[PARSER] 'channel' not a string");
            }
        }
        else {
            WK_WARN("[PARSER] 'channel' missing");
        }
    }


private:
    Context ctx_;
    simdjson::dom::parser parser_;

private:

    // =========================================================================
    // Parse helpers for method messages
    // =========================================================================

    [[nodiscard]] inline bool parse_method_message_(Method m, const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
        // Result object
        auto result_field = root["result"];
        if (result_field.error()) {
            WK_WARN("[PARSER] Field 'result' missing in <method> message -> ignore message.");
            return false;
        }
        dom::element result = result_field.value();
        // Channel field
        std::string_view channel_sv;
        auto channel_field = result["channel"];
        if (channel_field.error() || channel_field.get(channel_sv)) {
            WK_WARN("[PARSER] Field 'channel' missing or invalid in <method> message -> ignore message.");
            return false; // missing or invalid
        }
        Channel channel = to_channel_enum_fast(channel_sv);
        if (channel == Channel::Unknown) {
            WK_WARN("[PARSER] Unknown channel in <method> message -> ignore message.");
            return false; // reject unknown channels
        }
        switch (m) {
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
            default:
                WK_WARN("[PARSER] Subscription ACK parsing not implemented for channel '" << to_string(channel) << "'");
                break;
        }
        return false;
    }

    // UNSUBSCRIBE ACK PARSER
    [[nodiscard]] inline bool parse_unsubscribe_ack(Channel channel, const simdjson::dom::element& root) noexcept {
        switch (channel) {
            case Channel::Trade: {
                kraken::trade::UnsubscribeAck resp;
                if (parse_(root, resp)) {
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
            default:
                WK_WARN("[PARSER] Unsubscription ACK parsing not implemented for channel '" << to_string(channel) << "'");
                break;
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
                kraken::status::Update update;
                if (status::update::parse(root, update)) {
                    // TODO: push to ring / dispatch / callback
                    return true;
                }
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
    };

    // TICKER PARSER
    [[nodiscard]] inline bool parse_ticker_(const simdjson::dom::element& root) noexcept {
        using namespace simdjson;
        WK_WARN("[PARSER] Unhandled channel 'ticker' -> ignore");
        // TODO
        return false;
    };

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
        switch (to_payload_type(type.value())) {
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
    };





    [[nodiscard]] inline bool parse_(const simdjson::dom::element& root, kraken::trade::SubscribeAck& out) noexcept {
        using namespace simdjson;
        // required: success (boolean)
        {
            auto success_field = root["success"];
            if (success_field.error() || success_field.get(out.success)) {
                WK_WARN("[PARSER] Field 'success' missing in trade::SubscribeAck -> ignore message.");
                return false;  // required
            }
        }
        // -------- SUCCESS CASE --------
        if (out.success) {
            auto result_field = root["result"];
            if (result_field.error()) {
                WK_WARN("[PARSER] Field 'result' missing in trade::SubscribeAck -> ignore message.");
                return false;
            }
            dom::element result = result_field.value();
            // required: symbol
            std::string_view sv;
            if (result["symbol"].get(sv)) {
                WK_WARN("[PARSER] Field 'symbol' missing in trade::SubscribeAck -> ignore message.");
                return false;  // required field missing
            }
            out.symbol.assign(sv);
            // optional: snapshot
            bool snapshot_val = false;
            auto snap_field = result["snapshot"];
            if (!snap_field.error() && !snap_field.get(snapshot_val)) {
                out.snapshot = snapshot_val;
            }
            // optional: warnings[]
            auto warnings_field = result["warnings"];
            if (!warnings_field.error()) {
                dom::array arr;
                if (!warnings_field.get(arr)) {
                    std::vector<std::string> warnings_vec;
                    for (auto w : arr) {
                        std::string_view sv;
                        if (!w.get(sv))
                            warnings_vec.emplace_back(sv);
                    }
                    out.warnings = std::move(warnings_vec);
                }
            }
        }
        else { // -------- FAILURE CASE --------
            std::string_view sv;
            auto err_field = root["error"];
            if (!err_field.error() && !err_field.get(sv)) {
                out.error = std::string(sv);
            }
        }
        // optional: req_id
        uint64_t req_id_val = 0;
        auto req_field = root["req_id"];
        if (!req_field.error() && !req_field.get(req_id_val)) {
            out.req_id = req_id_val;
        }
        // timestamps (optional)
        std::string_view time_sv;
        auto tin = root["time_in"];
        if (!tin.error() && !tin.get(time_sv)) {
            wirekrak::Timestamp ts;
            if (wirekrak::parse_rfc3339(time_sv, ts)) {
                out.time_in = ts;
            }
        }
        auto tout = root["time_out"];
        if (!tout.error() && !tout.get(time_sv)) {
            wirekrak::Timestamp ts;
            if (wirekrak::parse_rfc3339(time_sv, ts)) {
                out.time_out = ts;
            }
        }
        return true;
    }



    inline bool parse_(const simdjson::dom::element& root, kraken::trade::UnsubscribeAck& out) noexcept {
        using namespace simdjson;
        out = {};
        // success (required)
        {
            auto f = root["success"];
            if (f.error() || f.get(out.success)) {
                WK_WARN("[PARSER] Field 'success' missing in trade::UnsubscribeAck -> ignore message.");
                return false;
            }
        }
        // -------- SUCCESS CASE --------
        auto result_field = root["result"];
        if (result_field.error()) {
            WK_WARN("[PARSER] Field 'result' missing in trade::UnsubscribeAck -> ignore message.");
            return false;
        }
        dom::element result = result_field.value();
        // Extract symbol (required)
        {
            std::string_view sv;
            if (result["symbol"].get(sv)) {
                WK_WARN("[PARSER] Field 'symbol' missing in trade::UnsubscribeAck -> ignore message.");
                return false;
            }
            out.symbol.assign(sv);
        }
        // -------- FAILURE CASE --------
        if (!out.success) {
            if (auto ef = root["error"]; !ef.error()) {
                std::string_view sv;
                if (!ef.get(sv)) {
                    out.error_msg = std::string(sv);
                }
            }
        }
        // req_id (optional)
        if (auto f = root["req_id"]; !f.error()) {
            auto r = f.get_uint64();
            if (!r.error()) {
                out.req_id = r.value();
            }
        }
        // timestamps (optional)
        if (auto f = root["time_in"]; !f.error()) {
            std::string_view sv;
            if (!f.get(sv)) {
                wirekrak::Timestamp ts;
                if (wirekrak::parse_rfc3339(sv, ts)) {
                    out.time_in = ts;
                }
            }
        }
        if (auto f = root["time_out"]; !f.error()) {
            std::string_view sv;
            if (!f.get(sv)) {
                wirekrak::Timestamp ts;
                if (wirekrak::parse_rfc3339(sv, ts)) {
                    out.time_out = ts;
                }
            }
        }
        return true;
    }

};

} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

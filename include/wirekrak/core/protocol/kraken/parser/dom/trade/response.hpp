#pragma once

#include <string_view>

#include "wirekrak/core/protocol/message_result.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/response.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/helpers.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/adapters.hpp"
#include "lcr/log/logger.hpp"

#include "simdjson.h"


namespace wirekrak::core::protocol::kraken::parser::dom::trade {

struct response {

    [[nodiscard]]
    static inline MessageResult parse(const simdjson::dom::element& root, schema::trade::Response& out) noexcept {
        out = schema::trade::Response{};

        // Root
        auto r = helper::require_object(root);
        if (r != MessageResult::Parsed) {
            WK_TRACE("[PARSER] Root not an object in trade response -> ignore message.");
            return r;
        }

        // type (required): snapshot | update
        r = adapter::parse_payload_type_required(root, "type", out.type);
        if (r != MessageResult::Parsed) {
            WK_TRACE("[PARSER] Field 'type' invalid or missing in trade response -> ignore message.");
            return r;
        }

        // data array (required)
        simdjson::dom::array data;
        r = helper::parse_array_required(root, "data", data);
        if (r != MessageResult::Parsed) {
            WK_TRACE("[PARSER] Field 'data' missing or invalid in trade response -> ignore message.");
            return r;
        }
    
        // data must contain at least one trade
        if (data.size() == 0) {
            WK_TRACE("[PARSER] Empty 'data' array in trade response -> ignore message.");
            return MessageResult::Ignored;
        }


        out.trades.clear();
        out.trades.reserve(data.size());

        // ------------------------------------------------------------
        // Parse trade objects
        // ------------------------------------------------------------
        for (const simdjson::dom::element& elem : data) {

            simdjson::dom::object obj;
            if (elem.get(obj)) {
                WK_TRACE("[PARSER] Data element not an object in trade response -> ignore message.");
                return MessageResult::InvalidSchema;
            }

            schema::trade::Trade trade{};

            // symbol (required)
            r = adapter::parse_symbol_required(obj, "symbol", trade.symbol);
            if (r != MessageResult::Parsed) {
                WK_TRACE("[PARSER] Field 'symbol' missing in trade object -> ignore message.");
                return r;
            }

            // side (required)
            r = adapter::parse_side_required(obj, "side", trade.side);
            if (r != MessageResult::Parsed) {
                WK_TRACE("[PARSER] Field 'side' missing in trade object -> ignore message.");
                return r;
            }

            // qty (required)
            r = helper::parse_double_required(obj, "qty", trade.qty);
            if (r != MessageResult::Parsed) {
                WK_TRACE("[PARSER] Field 'qty' missing or invalid in trade object -> ignore message.");
                return r;
            }

            // price (required)
            r = helper::parse_double_required(obj, "price", trade.price);
            if (r != MessageResult::Parsed) {
                WK_TRACE("[PARSER] Field 'price' missing or invalid in trade object -> ignore message.");
                return r;
            }

            // trade_id (required)
            r = helper::parse_uint64_required(obj, "trade_id", trade.trade_id);
            if (r != MessageResult::Parsed) {
                WK_TRACE("[PARSER] Field 'trade_id' missing or invalid in trade object -> ignore message.");
                return r;
            }

            // timestamp (required)
            r = adapter::parse_timestamp_required(obj, "timestamp", trade.timestamp);
            if (r != MessageResult::Parsed) {
                WK_TRACE("[PARSER] Field 'timestamp' missing or invalid in trade object -> ignore message.");
                return r;
            }

            // ord_type (optional)
            r = adapter::parse_order_type_optional(obj, "ord_type", trade.ord_type);
            if (r != MessageResult::Parsed) {
                WK_TRACE("[PARSER] Field 'ord_type' invalid in trade object -> ignore message.");
                return r;
            }

            out.trades.emplace_back(std::move(trade));
        }

        return MessageResult::Parsed;
    }
};

} // namespace wirekrak::core::protocol::kraken::parser::dom::trade

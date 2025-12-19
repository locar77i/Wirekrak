#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/trade/response.hpp"
#include "wirekrak/protocol/kraken/parser/helpers.hpp"
#include "wirekrak/protocol/kraken/parser/adapters.hpp"
#include "lcr/log/logger.hpp"

#include "simdjson.h"

namespace wirekrak::protocol::kraken::parser::trade {

struct response {

    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, kraken::trade::Response& out) noexcept {
        using namespace simdjson;

        // Root
        if (!helper::require_object(root)) {
            WK_DEBUG("[PARSER] Root not an object in trade response -> ignore message.");
            return false;
        }

        // type (required): snapshot | update
        if (!adapter::parse_payload_type_required(root, "type", out.type)) {
            WK_DEBUG("[PARSER] Field 'type' invalid or missing in trade response -> ignore message.");
            return false;
        }

        // data array (required)
        simdjson::dom::array data;
        if (!helper::parse_array_required(root, "data", data)) {
            WK_DEBUG("[PARSER] Field 'data' missing or invalid in trade response -> ignore message.");
            return false;
        }
    
        // data must contain at least one trade
        if (data.size() == 0) {
            WK_DEBUG("[PARSER] Empty 'data' array in trade response -> ignore message.");
            return false;
        }


        out.trades.clear();
        out.trades.reserve(data.size());

        // ------------------------------------------------------------
        // Parse trade objects
        // ------------------------------------------------------------
        for (const simdjson::dom::element& elem : data) {

            simdjson::dom::object obj;
            if (elem.get(obj)) {
                WK_DEBUG("[PARSER] Data element not an object in trade response -> ignore message.");
                return false;
            }

            kraken::trade::Trade trade{};

            // symbol (required)
            if (!adapter::parse_symbol_required(obj, "symbol", trade.symbol)) {
                WK_DEBUG("[PARSER] Field 'symbol' missing in trade object -> ignore message.");
                return false;
            }

            // side (required)
            if (!adapter::parse_side_required(obj, "side", trade.side)) {
                WK_DEBUG("[PARSER] Field 'side' missing in trade object -> ignore message.");
                return false;
            }

            // qty (required)
            if (!helper::parse_double_required(obj, "qty", trade.qty)) {
                WK_DEBUG("[PARSER] Field 'qty' missing or invalid in trade object -> ignore message.");
                return false;
            }

            // price (required)
            if (!helper::parse_double_required(obj, "price", trade.price)) {
                WK_DEBUG("[PARSER] Field 'price' missing or invalid in trade object -> ignore message.");
                return false;
            }

            // trade_id (required)
            if (!helper::parse_uint64_required(obj, "trade_id", trade.trade_id)) {
                WK_DEBUG("[PARSER] Field 'trade_id' missing or invalid in trade object -> ignore message.");
                return false;
            }

            // timestamp (required)
            if (!adapter::parse_timestamp_required(obj, "timestamp", trade.timestamp)) {
                WK_DEBUG("[PARSER] Field 'timestamp' missing or invalid in trade object -> ignore message.");
                return false;
            }

            // ord_type (optional)
            if (!adapter::parse_order_type_optional(obj, "ord_type", trade.ord_type)) {
                WK_DEBUG("[PARSER] Field 'ord_type' invalid in trade object -> ignore message.");
                return false;
            }

            out.trades.emplace_back(std::move(trade));
        }

        return true;
    }
};

} // namespace wirekrak::protocol::kraken::parser::trade

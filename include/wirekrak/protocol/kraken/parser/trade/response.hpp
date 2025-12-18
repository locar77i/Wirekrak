#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/trade/response.hpp"
#include "wirekrak/protocol/kraken/enums/channel.hpp"
#include "wirekrak/protocol/kraken/enums/side.hpp"
#include "wirekrak/protocol/kraken/enums/order_type.hpp"
#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/timestamp.hpp"
#include "lcr/log/logger.hpp"

#include "simdjson.h"

namespace wirekrak::protocol::kraken::parser::trade {

struct response {

    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, kraken::trade::Response& out) noexcept {
        using namespace simdjson;

        // ------------------------------------------------------------
        // channel (required)
        // ------------------------------------------------------------
        std::string_view channel_sv;
        if (root["channel"].get(channel_sv) || to_channel_enum_fast(channel_sv) != Channel::Trade) {
            WK_DEBUG("[PARSER] Invalid or missing 'channel' in trade message.");
            return false;
        }

        // ------------------------------------------------------------
        // type (required): snapshot | update
        // ------------------------------------------------------------
        std::string_view type_sv;
        if (root["type"].get(type_sv)) {
            WK_DEBUG("[PARSER] Missing 'type' in trade message.");
            return false;
        }

        if (type_sv == "snapshot") {
            out.type = kraken::trade::Type::Snapshot;
        } else if (type_sv == "update") {
            out.type = kraken::trade::Type::Update;
        } else {
            WK_DEBUG("[PARSER] Invalid 'type' in trade message.");
            return false;
        }

        // ------------------------------------------------------------
        // data array (required, >= 1)
        // ------------------------------------------------------------
        dom::array data;
        if (root["data"].get(data) || data.begin() == data.end()) {
            WK_DEBUG("[PARSER] Missing or empty 'data' array in trade message.");
            return false;
        }

        out.trades.clear();
        out.trades.reserve(data.size());

        // ------------------------------------------------------------
        // parse each trade object
        // ------------------------------------------------------------
        for (const dom::element& t : data) {

            dom::object obj;
            if (t.get(obj)) {
                WK_DEBUG("[PARSER] Invalid trade object in data[].");
                return false;
            }

            kraken::trade::Trade trade{};

            // symbol (required)
            std::string_view symbol_sv;
            if (obj["symbol"].get(symbol_sv)) {
                WK_DEBUG("[PARSER] Missing 'symbol' in trade object.");
                return false;
            }
            trade.symbol = Symbol{ std::string(symbol_sv) };

            // side (required)
            std::string_view side_sv;
            if (obj["side"].get(side_sv)) {
                WK_DEBUG("[PARSER] Missing 'side' in trade object.");
                return false;
            }
            trade.side = to_side_enum_fast(side_sv);

            // qty (required)
            if (obj["qty"].get(trade.qty)) {
                WK_DEBUG("[PARSER] Missing 'qty' in trade object.");
                return false;
            }

            // price (required)
            if (obj["price"].get(trade.price)) {
                WK_DEBUG("[PARSER] Missing 'price' in trade object.");
                return false;
            }

            // trade_id (required)
            if (obj["trade_id"].get(trade.trade_id)) {
                WK_DEBUG("[PARSER] Missing 'trade_id' in trade object.");
                return false;
            }

            // timestamp (required)
            std::string_view ts_sv;
            if (obj["timestamp"].get(ts_sv) ||
                !parse_rfc3339(ts_sv, trade.timestamp))
            {
                WK_DEBUG("[PARSER] Invalid or missing 'timestamp' in trade object.");
                return false;
            }

            // ord_type (optional)
            std::string_view ord_sv;
            if (!obj["ord_type"].get(ord_sv)) {
                trade.ord_type = to_order_type_enum_fast(ord_sv);
            }

            out.trades.emplace_back(std::move(trade));
        }

        return true;
    }
};

} // namespace wirekrak::protocol::kraken::parser::trade

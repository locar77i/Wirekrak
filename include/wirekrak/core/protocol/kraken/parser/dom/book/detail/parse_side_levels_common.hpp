#pragma once

#include "wirekrak/core/protocol/message_result.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/helpers.hpp"
#include "lcr/log/logger.hpp"

#include "simdjson.h"


namespace wirekrak::core::protocol::kraken::parser::dom::book::detail {

template<typename Levels>
[[nodiscard]]
inline MessageResult parse_side_levels_common(const simdjson::dom::object& book, std::string_view field, Levels& out_levels, bool& present) noexcept {
    present = false;

    auto levels = book[field];
    if (levels.error()) {
        WK_TRACE("[PARSER] Field '" << field << "' missing in book message -> skip side.");
        return MessageResult::Parsed; // optional → not an error
    }

    auto arr = levels.get_array();
    if (arr.error()) {
        WK_TRACE("[PARSER] Field '" << field << "' is not an array in book message -> ignore message.");
        return MessageResult::InvalidSchema;
    }

    present = true;
    for (auto lvl : arr.value()) {
        // each level must be an object with price and qty
        simdjson::dom::object obj;
        if (lvl.get(obj)) {
            WK_TRACE("[PARSER] Level entry in '" << field << "' is not an object -> ignore message.");
            return MessageResult::InvalidSchema;
        }
        // parse price and qty
        double price = 0.0;
        double qty   = 0.0;
        auto r1 = helper::parse_double_required(obj, "price", price);
        auto r2 = helper::parse_double_required(obj, "qty", qty);
        if (r1 != MessageResult::Parsed || r2 != MessageResult::Parsed) {
            WK_TRACE("[PARSER] Invalid level entry in '" << field << "' side -> ignore message.");
            return MessageResult::InvalidSchema;
        }
        out_levels.push_back({ price, qty });
    }

    return MessageResult::Parsed;
}

} // namespace wirekrak::core::protocol::kraken::parser::dom::book::detail

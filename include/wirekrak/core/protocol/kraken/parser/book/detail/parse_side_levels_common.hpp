#pragma once

#include "wirekrak/core/protocol/kraken/parser/result.hpp"
#include "wirekrak/core/protocol/kraken/parser/helpers.hpp"
#include "lcr/log/logger.hpp"

#include "simdjson.h"


namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace parser {
namespace detail {

template<typename Levels>
[[nodiscard]]
inline Result parse_side_levels_common(const simdjson::dom::object& book, std::string_view field, Levels& out_levels, bool& present) noexcept {
    present = false;

    auto levels = book[field];
    if (levels.error()) {
        WK_DEBUG("[PARSER] Field '" << field << "' missing in book message -> skip side.");
        return Result::Parsed; // optional → not an error
    }

    auto arr = levels.get_array();
    if (arr.error()) {
        WK_DEBUG("[PARSER] Field '" << field << "' is not an array in book message -> ignore message.");
        return Result::InvalidSchema;
    }

    present = true;
    for (auto lvl : arr.value()) {
        // each level must be an object with price and qty
        simdjson::dom::object obj;
        if (lvl.get(obj)) {
            WK_DEBUG("[PARSER] Level entry in '" << field << "' is not an object -> ignore message.");
            return Result::InvalidSchema;
        }
        // parse price and qty
        double price = 0.0;
        double qty   = 0.0;
        auto r1 = helper::parse_double_required(obj, "price", price);
        auto r2 = helper::parse_double_required(obj, "qty", qty);
        if (r1 != Result::Parsed || r2 != Result::Parsed) {
            WK_DEBUG("[PARSER] Invalid level entry in '" << field << "' side -> ignore message.");
            return Result::InvalidSchema;
        }
        out_levels.push_back({ price, qty });
    }

    return Result::Parsed;
}

} // namespace detail
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core

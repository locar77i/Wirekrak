#pragma once

#include "lcr/log/logger.hpp"

#include "simdjson.h"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace parser {
namespace detail {

template<typename Levels>
[[nodiscard]]
inline bool parse_side_common(const simdjson::dom::object& book, std::string_view field, Levels& out_levels, bool& present) noexcept {
    present = false;

    auto levels = book[field];
    if (levels.error()) {
        WK_DEBUG("[PARSER] Field '" << field << "' missing in book message -> skip side.");
        return true; // optional → not an error
    }

    auto arr = levels.get_array();
    if (arr.error()) {
        WK_DEBUG("[PARSER] Field '" << field << "' is not an array in book message -> skip side.");
        return false;
    }

    present = true;
    for (auto lvl : arr.value()) {
        auto price = lvl["price"].get_double();
        auto qty   = lvl["qty"].get_double();
        if (price.error() || qty.error()) {
            WK_DEBUG("[PARSER] Invalid level entry in '" << field << "' side -> ignore message.");
            return false;
        }

        out_levels.push_back({ price.value(), qty.value() });
    }

    return true;
}

} // namespace detail
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

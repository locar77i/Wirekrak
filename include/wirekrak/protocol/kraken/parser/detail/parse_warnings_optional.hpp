#pragma once

#include <string>
#include <vector>

#include "simdjson.h"
#include "lcr/log/logger.hpp"

namespace wirekrak::protocol::kraken::parser::detail {

// ------------------------------------------------------------
// Optional warnings[] parser
// ------------------------------------------------------------
// Schema:
//   "warnings": [ "string", ... ]
//
// Rules:
//   • Field is optional
//   • If present, must be an array of strings
//   • Any violation => parse failure
//   • Never throws
// ------------------------------------------------------------

inline bool parse_warnings_optional(const simdjson::dom::object& obj, std::vector<std::string>& out_warnings) noexcept
{
    auto field = obj["warnings"];
    if (field.error()) {
        // Optional field not present → OK
        return true;
    }

    simdjson::dom::array arr;
    if (field.get(arr)) {
        WK_DEBUG("[PARSER] Field 'warnings' is not an array.");
        return false;
    }

    for (auto w : arr) {
        std::string_view sv;
        if (w.get(sv)) {
            WK_DEBUG("[PARSER] Non-string element in 'warnings' array.");
            return false;
        }
        out_warnings.emplace_back(sv);
    }

    return true;
}

} // namespace wirekrak::protocol::kraken::parser::detail

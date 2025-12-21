#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/rejection_notice.hpp"
#include "wirekrak/protocol/kraken/parser/helpers.hpp"
#include "wirekrak/protocol/kraken/parser/adapters.hpp"
#include "lcr/log/logger.hpp"

#include "simdjson.h"

namespace wirekrak::protocol::kraken::parser {

struct rejection_notice {
    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, kraken::rejection::Notice& out) noexcept {
        out = kraken::rejection::Notice{};

        // Root must be object
        auto r = helper::require_object(root);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Root not an object in rejection notice -> ignore message.");
            return false;
        }

        // error (required)
        std::string_view sv;
        r = helper::parse_string_required(root, "error", sv);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Field 'error' missing in failed rejection notice -> ignore message.");
            return false;
        }
        out.error = std::string(sv);

        // req_id (optional, strict)
        r = helper::parse_uint64_optional(root, "req_id", out.req_id);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Field 'req_id' invalid in rejection notice -> ignore message.");
            return false;
        }

        // symbol (optional)
        r = adapter::parse_symbol_optional(root, "symbol", out.symbol);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Field 'symbol' invalid in rejection notice -> ignore message.");
            return false;
        }

        // timestamps (optional)
        r = adapter::parse_timestamp_optional(root, "time_in", out.time_in);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Field 'time_in' invalid in rejection notice -> ignore message.");
            return false;
        }

        r = adapter::parse_timestamp_optional(root, "time_out", out.time_out);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Field 'time_out' invalid in rejection notice -> ignore message.");
            return false;
        }

        return true;
    }
};

} // namespace wirekrak::protocol::kraken::parser

#pragma once

#include <string_view>

#include "simdjson.h"

#include "wirekrak/core/protocol/kraken/parser/result.hpp"
#include "wirekrak/core/protocol/kraken/parser/helpers.hpp"
#include "wirekrak/core/protocol/kraken/parser/adapters.hpp"
#include "wirekrak/core/protocol/kraken/channel_traits.hpp"
#include "lcr/log/logger.hpp"

#include "simdjson.h"


namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace parser {
namespace trade {
namespace detail {

template<typename Ack>
[[nodiscard]]
inline Result parse_ack_common(const simdjson::dom::element& root, std::string_view expected_method, Ack& out) noexcept {
    // Root must be object
    auto r = helper::require_object(root);
    if (r != Result::Parsed) {
        WK_DEBUG("[PARSER] Root not an object in " << expected_method << " ACK -> ignore message.");
        return r;
    }

/* Enforced by caller/router
    // method (required)
    if (!helper::parse_string_equals_required(root, "method", expected_method)) {
        WK_DEBUG("[PARSER] Field 'method' missing or invalid in " << expected_method << " ACK -> ignore message.");
        return false;
    }
*/
    // success (required)
    r = helper::parse_bool_required(root, "success", out.success);
    if (r != Result::Parsed) {
        WK_DEBUG("[PARSER] Field 'success' missing in " << expected_method << " ACK -> ignore message.");
        return r;
    }

    // ============================================================
    // SUCCESS CASE
    // ============================================================
    if (out.success) {

        // result object (required)
        simdjson::dom::element result;
        r = helper::parse_object_required(root, "result", result);
        if (r != Result::Parsed) {
            WK_WARN("[PARSER] Field 'result' missing or invalid in '" << expected_method << "' message -> ignore message.");
            return r;
        }

/* Enforced by caller/router
        // channel (required)
        if (!helper::parse_string_equals_required(result, "channel", channel_name_of_v<Ack>)) {
            WK_DEBUG("[PARSER] Field 'channel' missing in " << expected_method << " ACK -> ignore message.");
            return false;
        }
*/

        // symbol (required)
        r = adapter::parse_symbol_required(result, "symbol", out.symbol);
        if (r != Result::Parsed) {
            WK_DEBUG("[PARSER] Field 'symbol' missing in " << expected_method << " ACK -> ignore message.");
            return r;
        }

        // snapshot (subscribe-only)
        if constexpr (requires { out.snapshot; }) {
            // snapshot (optional)
            r = helper::parse_bool_optional(result, "snapshot", out.snapshot);
            if (r != Result::Parsed) {
                WK_DEBUG("[PARSER] Field 'snapshot' invalid in " << expected_method << " ACK -> ignore message.");
                return r;
            }
        }

        // warnings (subscribe-only)
        if constexpr (requires { out.warnings; }) {
            // warnings (optional, strict)
            bool presence;
            r = helper::parse_string_list_optional(result, "warnings", out.warnings, presence);
            if (r != Result::Parsed) {
                WK_DEBUG("[PARSER] Field 'warnings' invalid in " << expected_method << " ACK -> ignore message.");
                return r;
            }
        }

        // error must NOT exist
        if (!root["error"].error()) {
            WK_DEBUG("[PARSER] Field 'error' present in successful " << expected_method << " ACK -> ignore message.");
            return Result::InvalidSchema;
        }
    }
    // ============================================================
    // FAILURE CASE
    // ============================================================
    else {

        // error (conditionally required) - parse it as required in the failure branch
        std::string_view sv;
        r = helper::parse_string_required(root, "error", sv);
        if (r != Result::Parsed) {
            WK_DEBUG("[PARSER] Field 'error' missing in failed " << expected_method << " ACK -> ignore message.");
            return r;
        }
        out.error = std::string(sv);

        // result must NOT exist
        if (!root["result"].error()) {
            WK_DEBUG("[PARSER] Field 'result' present in failed " << expected_method << " ACK -> ignore message.");
            return Result::InvalidSchema;
        }
    }

    // req_id (optional, strict)
    r = helper::parse_uint64_optional(root, "req_id", out.req_id);
    if (r != Result::Parsed) {
        WK_DEBUG("[PARSER] Field 'req_id' invalid in " << expected_method << " ACK -> ignore message.");
        return r;
    }

    // timestamps (optional)
    r = adapter::parse_timestamp_optional(root, "time_in", out.time_in);
    if (r != Result::Parsed) {
        WK_DEBUG("[PARSER] Field 'time_in' invalid in " << expected_method << " ACK -> ignore message.");
        return r;
    }

    r = adapter::parse_timestamp_optional(root, "time_out", out.time_out);
    if (r != Result::Parsed) {
        WK_DEBUG("[PARSER] Field 'time_out' invalid in " << expected_method << " ACK -> ignore message.");
        return r;
    }

    return Result::Parsed;
}

} // namespace detail
} // namespace trade
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core

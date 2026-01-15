#pragma once

#include <string_view>

#include "wirekrak/core/protocol/kraken/parser/result.hpp"
#include "wirekrak/core/protocol/kraken/parser/helpers.hpp"
#include "wirekrak/core/protocol/kraken/parser/adapters.hpp"
#include "wirekrak/core/protocol/kraken/channel_traits.hpp"
#include "lcr/log/logger.hpp"

#include "simdjson.h"


namespace wirekrak::core::protocol::kraken::parser::detail {

template<typename Ack>
[[nodiscard]]
inline bool parse_ack_common(const simdjson::dom::element& root, std::string_view expected_method, Ack& out) noexcept{
    using namespace simdjson;

    // Root
    auto r = helper::require_object(root);
    if (r != parser::Result::Ok) {
        WK_DEBUG("[PARSER] Root not an object in " << expected_method << " ACK -> ignore message.");
        return false;
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
    if (r != parser::Result::Ok) {
        WK_DEBUG("[PARSER] Field 'success' missing in " << expected_method << " ACK -> ignore message.");
        return false;
    }

    // SUCCESS CASE
    if (out.success) {

        // result object (required)
        simdjson::dom::element result;
        r = helper::parse_object_required(root, "result", result);
        if (r != parser::Result::Ok) {
            WK_WARN("[PARSER] Field 'result' missing or invalid in '" << expected_method << "' message -> ignore message.");
            return false;
        }

/* Enforced by caller/router
        // channel (required)
        if (!helper::parse_string_equals_required(result, "channel", channel_name_of_v<Ack>)) {
            WK_DEBUG("[PARSER] Field 'channel' missing in " << expected_method << " ACK -> ignore message.");
            return false;
        }
*/

        // symbol (required)
        r = parser::adapter::parse_symbol_required(result, "symbol", out.symbol);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Field 'symbol' missing in " << expected_method << " ACK -> ignore message.");
            return false;
        }

        // depth (required)
        std::uint64_t depth = 0;
        r = helper::parse_uint64_required(result, "depth", depth);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Field 'depth' missing in " << expected_method << " ACK -> ignore message.");
            return false;
        }
        out.depth = static_cast<std::uint32_t>(depth);

        // snapshot (subscribe-only)
        if constexpr (requires { out.snapshot; }) {
            r = helper::parse_bool_optional(result, "snapshot", out.snapshot);
            if (r != parser::Result::Ok) {
                WK_DEBUG("[PARSER] Field 'snapshot' invalid in " << expected_method << " ACK -> ignore message.");
                return false;
            }
        }

        // warnings (subscribe-only, optional)
        if constexpr (requires { out.warnings; }) {
            bool presence;
            r = helper::parse_string_list_optional(result, "warnings", out.warnings, presence);
            if (r != parser::Result::Ok) {
                WK_DEBUG("[PARSER] Field 'warnings' invalid in " << expected_method << " ACK -> ignore message.");
                return false;
            }
        }

        // error must NOT exist on success
        if (!root["error"].error()) {
            WK_DEBUG("[PARSER] Field 'error' present in successful " << expected_method << " ACK -> ignore message.");
            return false;
        }
    }

    // FAILURE CASE
    else {
        std::string_view err;
        r = helper::parse_string_required(root, "error", err);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Field 'error' missing in failed " << expected_method << " ACK -> ignore message.");
            return false;
        }
        out.error = std::string(err);

/* TODO: more strict??
        // result must NOT exist on failure
        if (!root["result"].error()) {
            WK_DEBUG("[PARSER] Field 'result' present in failed " << expected_method << " ACK -> ignore message.");
            return false;
        }
 */
    }

    // req_id (optional)
    r = helper::parse_uint64_optional(root, "req_id", out.req_id);
    if (r != parser::Result::Ok) {
        WK_DEBUG("[PARSER] Field 'req_id' invalid in " << expected_method << " ACK -> ignore message.");
        return false;
    }

    // timestamps (optional)
    r = adapter::parse_timestamp_optional(root, "time_in", out.time_in);
    if (r != parser::Result::Ok) {
        WK_DEBUG("[PARSER] Field 'time_in' invalid in " << expected_method << " ACK -> ignore message.");
        return false;
    }

    r = adapter::parse_timestamp_optional(root, "time_out", out.time_out);
    if (r != parser::Result::Ok) {
        WK_DEBUG("[PARSER] Field 'time_out' invalid in " << expected_method << " ACK -> ignore message.");
        return false;
    }

    return true;
}

} // namespace wirekrak::core::protocol::kraken::parser::detail

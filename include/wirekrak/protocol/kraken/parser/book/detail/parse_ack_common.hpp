#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/parser/helpers.hpp"
#include "wirekrak/protocol/kraken/parser/adapters.hpp"
#include "wirekrak/protocol/kraken/channel_traits.hpp"
#include "lcr/log/logger.hpp"

#include "simdjson.h"


namespace wirekrak::protocol::kraken::parser::detail {

template<typename Ack>
[[nodiscard]]
inline bool parse_ack_common(const simdjson::dom::element& root, std::string_view expected_method, Ack& out) noexcept{
    using namespace simdjson;

    // Root
    if (!helper::require_object(root)) {
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
    if (!helper::parse_bool_required(root, "success", out.success)) {
        WK_DEBUG("[PARSER] Field 'success' missing in " << expected_method << " ACK -> ignore message.");
        return false;
    }

    // SUCCESS CASE
    if (out.success) {

        // result object (required)
        simdjson::dom::element result;
        if (!helper::parse_object_required(root, "result", result)) {
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
        if (!adapter::parse_symbol_required(result, "symbol", out.symbol)) {
            WK_DEBUG("[PARSER] Field 'symbol' missing in " << expected_method << " ACK -> ignore message.");
            return false;
        }

        // depth (required)
        std::uint64_t depth = 0;
        if (!helper::parse_uint64_required(result, "depth", depth)) {
            WK_DEBUG("[PARSER] Field 'depth' missing in " << expected_method << " ACK -> ignore message.");
            return false;
        }
        out.depth = static_cast<std::uint32_t>(depth);

        // snapshot (subscribe-only)
        if constexpr (requires { out.snapshot; }) {
            if (!helper::parse_bool_required(result, "snapshot", out.snapshot)) {
                WK_DEBUG("[PARSER] Field 'snapshot' missing in " << expected_method << " ACK -> ignore message.");
                return false;
            }
        }

        // warnings (subscribe-only, optional)
        if constexpr (requires { out.warnings; }) {
            if (!helper::parse_string_list_optional(result, "warnings", out.warnings)) {
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
        if (!helper::parse_string_required(root, "error", err)) {
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
    if (!helper::parse_uint64_optional(root, "req_id", out.req_id)) {
        WK_DEBUG("[PARSER] Field 'req_id' invalid in " << expected_method << " ACK -> ignore message.");
        return false;
    }

    // timestamps (optional)
    if (!adapter::parse_timestamp_optional(root, "time_in", out.time_in)) {
        WK_DEBUG("[PARSER] Field 'time_in' invalid in " << expected_method << " ACK -> ignore message.");
        return false;
    }

    if (!adapter::parse_timestamp_optional(root, "time_out", out.time_out)) {
        WK_DEBUG("[PARSER] Field 'time_out' invalid in " << expected_method << " ACK -> ignore message.");
        return false;
    }

    return true;
}

} // namespace wirekrak::protocol::kraken::parser::detail

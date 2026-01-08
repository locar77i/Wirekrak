#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/schema/system/pong.hpp"
#include "wirekrak/protocol/kraken/parser/helpers.hpp"
#include "wirekrak/protocol/kraken/parser/adapters.hpp"
#include "lcr/log/logger.hpp"

#include "simdjson.h"

namespace wirekrak::protocol::kraken::parser::system {

struct pong {

    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, schema::system::Pong& out) noexcept {
        using namespace simdjson;

/* Kraken API doc says:
        // Root must be an object
        auto r = helper::require_object(root);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Root not an object in pong response -> ignore message.");
            return false;
        }

        // success (required)
        r = helper::parse_bool_required(root, "success", out.success);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Field 'success' missing or invalid in pong response -> ignore message.");
            return false;
        }

        // req_id (optional)
        r = helper::parse_uint64_optional(root, "req_id", out.req_id);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Field 'req_id' invalid in pong response -> ignore message.");
            return false;
        }

        // SUCCESS CASE
        if (out.success) {

            // result object (required)
            simdjson::dom::element result;
            r = helper::parse_object_required(root, "result", result);
            if (r != parser::Result::Ok) {
                WK_WARN("[PARSER] Field 'result' missing or invalid in pong response -> ignore message.");
                return false;
            }

            // warnings (optional, strict)
            bool presence;
            r = helper::parse_string_list_optional(result, "warnings", out.warnings, presence);
            if (r != parser::Result::Ok) {
                WK_DEBUG("[PARSER] Field 'warnings' invalid in pong response -> ignore message.");
                return false;
            }

            // time_in (optional)
            r = adapter::parse_timestamp_optional(root, "time_in", out.time_in);
            if (r != parser::Result::Ok) {
                WK_DEBUG("[PARSER] Field 'time_in' invalid in pong response -> ignore message.");
                return false;
            }

            // time_out (optional)
            r = adapter::parse_timestamp_optional(root, "time_out", out.time_out);
            if (r != parser::Result::Ok) {
                WK_DEBUG("[PARSER] Field 'time_out' invalid in pong response -> ignore message.");
                return false;
            }

        }
        // FAILURE CASE
        else {
            std::string_view sv;
            r = helper::parse_string_required(root, "error", sv);
            if (r != parser::Result::Ok) {
                WK_DEBUG("[PARSER] Field 'error' missing in failed pong response -> ignore message.");
                return false;
            }
            out.error = std::string(sv);
        }
      
        return true;
*/
        // But reality is:

        // Root must be an object
        auto r = helper::require_object(root);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Root not an object in pong response -> ignore message.");
            return false;
        }

        // req_id (optional)
        r = helper::parse_uint64_optional(root, "req_id", out.req_id);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Field 'req_id' invalid in pong response -> ignore message.");
            return false;
        }

        // time_in (optional)
        r = adapter::parse_timestamp_optional(root, "time_in", out.time_in);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Field 'time_in' invalid in pong response -> ignore message.");
            return false;
        }

        // time_out (optional)
        r = adapter::parse_timestamp_optional(root, "time_out", out.time_out);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Field 'time_out' invalid in pong response -> ignore message.");
            return false;
        }

        // success (optional for pong)
        r = helper::parse_bool_optional(root, "success", out.success);
        if (r != parser::Result::Ok) {
            WK_DEBUG("[PARSER] Field 'success' invalid in pong response -> ignore message.");
            return false;
        }

        // Handle success / failure based on presence of 'success' field
        if (out.success.has()) {
            // SUCCESS CASE
            if (out.success.value()) {

                // result object (required)
                simdjson::dom::element result;
                r = helper::parse_object_required(root, "result", result);
                if (r != parser::Result::Ok) {
                    WK_WARN("[PARSER] Field 'result' missing or invalid in pong response -> ignore message.");
                    return false;
                }

                // warnings (optional, strict)
                bool presence;
                r = helper::parse_string_list_optional(result, "warnings", out.warnings, presence);
                if (r != parser::Result::Ok) {
                    WK_DEBUG("[PARSER] Field 'warnings' invalid in pong response -> ignore message.");
                    return false;
                }
            }
            // FAILURE CASE
            else {
                std::string_view sv;
                r = helper::parse_string_required(root, "error", sv);
                if (r != parser::Result::Ok) {
                    WK_DEBUG("[PARSER] Field 'error' missing in failed pong response -> ignore message.");
                    return false;
                }
                out.error = std::string(sv);
            }
        }
        
        return true;
    }
};

} // namespace wirekrak::protocol::kraken::parser::system

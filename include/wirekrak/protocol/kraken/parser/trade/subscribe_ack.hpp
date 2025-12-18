#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/trade/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/enums/channel.hpp"
#include "wirekrak/protocol/kraken/parser/detail/parse_warnings_optional.hpp"
#include "wirekrak/core/timestamp.hpp"
#include "lcr/log/logger.hpp"

#include "simdjson.h"

namespace wirekrak::protocol::kraken::parser::trade {

struct subscribe_ack {
    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, kraken::trade::SubscribeAck& out) noexcept
    {
        using namespace simdjson;

        if (root.type() != dom::element_type::OBJECT) {
            WK_DEBUG("[PARSER] trade::SubscribeAck root is not an object.");
            return false;
        }

        // success (required)
        if (root["success"].get(out.success)) {
            WK_DEBUG("[PARSER] Missing or invalid 'success' in trade::SubscribeAck.");
            return false;
        }

        if (out.success) {
            // forbid error on success
            if (!root["error"].error()) {
                WK_DEBUG("[PARSER] 'error' present in successful trade::SubscribeAck.");
                return false;
            }

            dom::object result;
            if (root["result"].get(result)) {
                WK_DEBUG("[PARSER] Missing 'result' in successful trade::SubscribeAck.");
                return false;
            }

            std::string_view sv;
            if (result["symbol"].get(sv)) {
                WK_DEBUG("[PARSER] Missing 'symbol' in trade::SubscribeAck result.");
                return false;
            }
            out.symbol = Symbol{std::string(sv)};

            // optional: snapshot
            bool snapshot_val = false;
            auto snap_field = result["snapshot"];
            if (!snap_field.error() && !snap_field.get(snapshot_val)) {
                out.snapshot = snapshot_val;
            }

            // optional: warnings[]
            if (!detail::parse_warnings_optional(result, out.warnings)) {
                return false;
            }
        }
        else {
            // forbid result on failure
            if (!root["result"].error()) {
                WK_DEBUG("[PARSER] 'result' present in failed trade::SubscribeAck.");
                return false;
            }

            std::string_view sv;
            if (!root["error"].get(sv)) {
                out.error = std::string(sv);
            }
        }

        // req_id (optional, but strict)
        std::uint64_t req_id = 0;
        auto req_field = root["req_id"];
        if (!req_field.error()) {
            if (req_field.get(req_id)) {
                WK_DEBUG("[PARSER] Invalid 'req_id' type in trade::SubscribeAck.");
                return false;
            }
            out.req_id = req_id;
        }

        // timestamps (optional)
        std::string_view ts_sv;
        auto parse_ts = [&](const char* key, lcr::optional<Timestamp>& out_ts) {
            if (!root[key].get(ts_sv)) {
                Timestamp ts;
                if (parse_rfc3339(ts_sv, ts))
                    out_ts = ts;
            }
        };

        parse_ts("time_in",  out.time_in);
        parse_ts("time_out", out.time_out);

        return true;
    }

};

} // namespace wirekrak::protocol::kraken::parser::trade

#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/enums/channel.hpp"
#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/timestamp.hpp"
#include "lcr/log/logger.hpp"

#include "simdjson.h"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace parser {
namespace detail {

template<typename Ack>
[[nodiscard]]
inline bool parse_ack_common(const simdjson::dom::element& root, std::string_view expected_method, Ack& out) noexcept {
    // method
    auto method = root["method"].get_string();
    if (method.error() || method.value() != expected_method)
        return false;

    // required: success (boolean)
    {
        auto success_field = root["success"];
        if (success_field.error() || success_field.get(out.success)) {
            WK_DEBUG("[PARSER] Field 'success' missing in trade::SubscribeAck -> ignore message.");
            return false;  // required
        }
    }
    // -------- SUCCESS CASE --------
    // result object
    auto result = root["result"].get_object();
    if (result.error()) {
        WK_DEBUG("[PARSER] Field 'result' missing in " << expected_method << " ACK -> ignore message.");
        return false;
    }

    // channel
    auto channel = result.value()["channel"].get_string();
    if (channel.error() || to_channel_enum_fast(channel.value()) != Channel::Book) {
        WK_DEBUG("[PARSER] Field 'channel' missing or invalid in " << expected_method << " ACK -> ignore message.");
        return false;
    }

    // symbol
    auto symbol = result.value()["symbol"].get_string();
    if (symbol.error()) {
        WK_DEBUG("[PARSER] Field 'symbol' missing in " << expected_method << " ACK -> ignore message.");
        return false;
    }
    out.symbol = Symbol{ std::string(symbol.value()) };

    // depth
    auto depth = result.value()["depth"].get_uint64();
    if (depth.error()) {
        WK_DEBUG("[PARSER] Field 'depth' missing in " << expected_method << " ACK -> ignore message.");
        return false;
    }
    out.depth = static_cast<std::uint32_t>(depth.value());

    // snapshot (subscribe-only)
    if constexpr (requires { out.snapshot; }) {
        auto snapshot = result.value()["snapshot"].get_bool();
        if (snapshot.error()) {
            WK_DEBUG("[PARSER] Field 'snapshot' missing in " << expected_method << " ACK -> ignore message.");
            return false;
        }
        out.snapshot = snapshot.value();
    }

    // warnings (subscribe-only)
    if constexpr (requires { out.warnings; }) {
        auto warnings = result.value()["warnings"];
        if (!warnings.error()) {
            for (auto w : warnings.get_array())
                out.warnings.emplace_back(std::string(w.get_string().value()));
        }
    }

    if (!out.success) { // -------- FAILURE CASE --------
        std::string_view sv;
        auto err_field = root["error"];
        if (!err_field.error() && !err_field.get(sv)) {
            out.error = std::string(sv);
        }
    }
    
    // req_id (optional)
    uint64_t req_id_val = 0;
    auto req_field = root["req_id"];
    if (!req_field.error() && !req_field.get(req_id_val)) {
        out.req_id = req_id_val;
    }

    // timestamps (optional)
    std::string_view sv;
    auto tin = root["time_in"];
    if (!tin.error() && !tin.get(sv)) {
        Timestamp ts;
        if (parse_rfc3339(sv, ts))
            out.time_in = ts;
    }

    auto tout = root["time_out"];
    if (!tout.error() && !tout.get(sv)) {
        Timestamp ts;
        if (parse_rfc3339(sv, ts))
            out.time_out = ts;
    }

    return true;
}


} // namespace detail
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

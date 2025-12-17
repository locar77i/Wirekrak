#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/enums/channel.hpp"
#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/timestamp.hpp"

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

    // result object
    auto result = root["result"].get_object();
    if (result.error())
        return false;

    // channel
    auto channel = result.value()["channel"].get_string();
    if (channel.error() ||
        to_channel_enum_fast(channel.value()) != Channel::Book)
        return false;

    // symbol
    auto symbol = result.value()["symbol"].get_string();
    if (symbol.error())
        return false;
    out.symbol = Symbol{ std::string(symbol.value()) };

    // depth
    auto depth = result.value()["depth"].get_uint64();
    if (depth.error())
        return false;
    out.depth = static_cast<std::uint32_t>(depth.value());

    // success
    auto success = result.value()["success"].get_bool();
    if (success.error())
        return false;
    out.success = success.value();

    // error (conditional)
    if (!out.success) {
        auto err = result.value()["error"].get_string();
        if (!err.error())
            out.error = std::string(err.value());
    }

    // snapshot (subscribe-only)
    if constexpr (requires { out.snapshot; }) {
        auto snapshot = result.value()["snapshot"].get_bool();
        if (snapshot.error())
            return false;
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

    // req_id
    auto req_id = result.value()["req_id"];
    if (!req_id.error())
        out.req_id = req_id.get_uint64().value();

    return true;
}


} // namespace detail
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

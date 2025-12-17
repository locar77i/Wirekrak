#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/book/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/enums/channel.hpp"

#include "simdjson.h"

namespace wirekrak {
namespace protocol {
namespace kraken {
namespace parser {
namespace book {

struct subscribe_ack {

    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, kraken::book::SubscribeAck& out) noexcept {
        // method must be "subscribe"
        auto method = root["method"].get_string();
        if (method.error() || method.value() != "subscribe")
            return false;

        auto result = root["result"].get_object();
        if (result.error())
            return false;

        // channel must be "book"
        auto channel = result.value()["channel"].get_string();
        if (channel.error() ||
            to_channel_enum_fast(channel.value()) != Channel::Book)
            return false;

        // Required fields
        auto symbol = result.value()["symbol"].get_string();
        if (symbol.error()) {
            return false;
        }
        out.symbol = Symbol{ std::string(symbol.value()) };

        auto depth = result.value()["depth"].get_uint64();
        if (depth.error()) {
            return false;
        }
        out.depth = static_cast<std::uint32_t>(depth.value());

        auto snapshot = result.value()["snapshot"].get_bool();
        if (snapshot.error()) {
            return false;
        }
        out.snapshot = snapshot.value();

        auto success = result.value()["success"].get_bool();
        if (success.error()) {
            return false;
        }
        out.success = success.value();

        // Optional error
        if (!out.success) {
            auto err = result.value()["error"].get_string();
            if (!err.error())
                out.error = std::string(err.value());
        }

        // Optional warnings
        auto warnings = result.value()["warnings"];
        if (!warnings.error()) {
            for (auto w : warnings.get_array()) {
                out.warnings.emplace_back(std::string(w.get_string().value()));
            }
        }

        // timestamps (optional)
        std::string_view time_sv;
        auto tin = root["time_in"];
        if (!tin.error() && !tin.get(time_sv)) {
            wirekrak::Timestamp ts;
            if (wirekrak::parse_rfc3339(time_sv, ts)) {
                out.time_in = ts;
            }
        }
        auto tout = root["time_out"];
        if (!tout.error() && !tout.get(time_sv)) {
            wirekrak::Timestamp ts;
            if (wirekrak::parse_rfc3339(time_sv, ts)) {
                out.time_out = ts;
            }
        }

        // Optional req_id
        auto req_id = result.value()["req_id"];
        if (!req_id.error())
            out.req_id = req_id.get_uint64().value();

        return true;
    }
};

} // namespace book
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

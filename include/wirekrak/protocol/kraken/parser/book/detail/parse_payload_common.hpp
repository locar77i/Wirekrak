#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/enums/channel.hpp"
#include "wirekrak/protocol/kraken/book/snapshot.hpp"
#include "wirekrak/protocol/kraken/book/update.hpp"
#include "wirekrak/protocol/kraken/parser/book/detail/parse_side_common.hpp"
#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/timestamp.hpp"
#include "lcr/log/logger.hpp"

#include "simdjson.h"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace parser {
namespace detail {

template<typename BookMsg>
[[nodiscard]]
inline bool parse_payload_common(const simdjson::dom::element& root, std::string_view expected_type, BookMsg& out) noexcept {
    // channel
    auto channel = root["channel"].get_string();
    if (channel.error() || to_channel_enum_fast(channel.value()) != Channel::Book) {
        WK_DEBUG("[PARSER] Field 'channel' missing or invalid in book message -> ignore message.");
        return false;
    }

    // type
    auto type = root["type"].get_string();
    if (type.error() || type.value() != expected_type) {
        WK_DEBUG("[PARSER] Field 'type' missing or invalid in book message -> ignore message.");
        return false;
    }

    // data array
    auto data = root["data"].get_array();
    if (data.error() || data.value().size() != 1) {
        WK_DEBUG("[PARSER] Field 'data' missing or invalid in book message -> ignore message.");
        return false;
    }

    auto book = data.value().at(0).get_object();
    if (book.error()) {
        WK_DEBUG("[PARSER] Field 'data[0]' missing or invalid in book message -> ignore message.");
        return false;
    }

    // symbol
    auto symbol = book.value()["symbol"].get_string();
    if (symbol.error()) {
        WK_DEBUG("[PARSER] Field 'symbol' missing in book message -> ignore message.");
        return false;
    }
    out.symbol = Symbol{ std::string(symbol.value()) };

    bool has_asks = false;
    if (!detail::parse_side_common(book.value(), "asks", out.asks, has_asks))
        return false;

    bool has_bids = false;
    if (!detail::parse_side_common(book.value(), "bids", out.bids, has_bids))
        return false;

    // Enforce Kraken rule: at least one side present
    if (!has_asks && !has_bids) {
        WK_DEBUG("[PARSER] Both sides 'asks' and 'bids' missing in book message -> ignore message.");
        return false;
    }

    // checksum
    auto checksum = book.value()["checksum"].get_uint64();
    if (checksum.error()) {
        WK_DEBUG("[PARSER] Field 'checksum' missing or invalid in book message -> ignore message.");
        return false;
    }
    out.checksum = static_cast<std::uint32_t>(checksum.value());

    // timestamp (only for Update)
    if constexpr (requires { out.timestamp; }) {
        std::string_view sv;
        auto ts = book.value()["timestamp"];
        if (ts.error() || ts.get(sv)) {
            WK_DEBUG("[PARSER] Field 'timestamp' missing or invalid in book message -> ignore message.");
            return false;
        }
        
        if (!parse_rfc3339(sv, out.timestamp)) {
            WK_DEBUG("[PARSER] Field 'timestamp' invalid in book message -> ignore message.");
            return false;
        }
    }

    return true;
}

} // namespace detail
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/enums/channel.hpp"
#include "wirekrak/protocol/kraken/book/snapshot.hpp"
#include "wirekrak/protocol/kraken/book/update.hpp"
#include "wirekrak/protocol/kraken/parser/detail/parse_book_levels.hpp"
#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/timestamp.hpp"

#include "simdjson.h"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace parser {
namespace detail {

template<typename BookMsg>
[[nodiscard]]
inline bool parse_book_payload_common(const simdjson::dom::element& root, std::string_view expected_type, BookMsg& out) noexcept {
    // channel
    auto channel = root["channel"].get_string();
    if (channel.error() ||
        to_channel_enum_fast(channel.value()) != Channel::Book)
        return false;

    // type
    auto type = root["type"].get_string();
    if (type.error() || type.value() != expected_type)
        return false;

    // data array
    auto data = root["data"].get_array();
    if (data.error() || data.value().size() != 1)
        return false;

    auto book = data.value().at(0).get_object();
    if (book.error())
        return false;

    // symbol
    auto symbol = book.value()["symbol"].get_string();
    if (symbol.error())
        return false;
    out.symbol = Symbol{ std::string(symbol.value()) };

    bool has_asks = false;
    if (!detail::parse_book_levels(book.value(), "asks", out.asks, has_asks))
        return false;

    bool has_bids = false;
    if (!detail::parse_book_levels(book.value(), "bids", out.bids, has_bids))
        return false;

    // Enforce Kraken rule: at least one side present
    if (!has_asks && !has_bids)
        return false;

    // checksum
    auto checksum = book.value()["checksum"].get_uint64();
    if (checksum.error())
        return false;
    out.checksum = static_cast<std::uint32_t>(checksum.value());

    // timestamp (only for Update)
    if constexpr (requires { out.timestamp; }) {
        std::string_view sv;
        auto ts = book.value()["timestamp"];
        if (ts.error() || ts.get(sv))
            return false;

        if (!parse_rfc3339(sv, out.timestamp))
            return false;
    }

    return true;
}

} // namespace detail
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

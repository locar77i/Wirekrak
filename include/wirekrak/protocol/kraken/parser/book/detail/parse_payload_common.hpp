#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/enums/payload_type.hpp"
#include "wirekrak/protocol/kraken/book/snapshot.hpp"
#include "wirekrak/protocol/kraken/book/update.hpp"
#include "wirekrak/protocol/kraken/parser/helpers.hpp"
#include "wirekrak/protocol/kraken/parser/adapters.hpp"
#include "wirekrak/protocol/kraken/parser/book/detail/parse_side_levels_common.hpp"
#include "lcr/log/logger.hpp"

#include "simdjson.h"

namespace wirekrak::protocol::kraken::parser::detail {

template<typename BookMsg>
[[nodiscard]]
inline bool parse_payload_common(const simdjson::dom::element& root, std::string_view expected_type, BookMsg& out) noexcept {
    using namespace simdjson;

    // Root
    if (!helper::require_object(root)) {
        WK_DEBUG("[PARSER] Root not an object in book message -> ignore message.");
        return false;
    }

    // type (required): snapshot | update
    kraken::PayloadType type;
    if (!adapter::parse_payload_type_required(root, "type", type)) {
        WK_DEBUG("[PARSER] Field 'type' invalid or missing in trade response -> ignore message.");
        return false;
    }

    // data array (required, exactly one element)
    simdjson::dom::array data;
    if (!helper::parse_array_required(root, "data", data)) {
        WK_DEBUG("[PARSER] Field 'data' missing or invalid in book message -> ignore message.");
        return false;
    }

    // enforce array size (exactly one element)
    if (data.size() != 1) {
        WK_DEBUG("[PARSER] Field 'data' does not contain exactly one element in book message -> ignore message.");
        return false;
    }

    simdjson::dom::object book;
    if (data.at(0).get(book)) {
        WK_DEBUG("[PARSER] Field 'data[0]' invalid in book message -> ignore message.");
        return false;
    }

    // symbol (required)
    if (!adapter::parse_symbol_required(book, "symbol", out.symbol)) {
        WK_DEBUG("[PARSER] Field 'symbol' missing in book message -> ignore message.");
        return false;
    }

    // sides (asks / bids)
    bool has_asks = false;
    if (!parse_side_levels_common(book, "asks", out.asks, has_asks))
        return false;

    bool has_bids = false;
    if (!parse_side_levels_common(book, "bids", out.bids, has_bids))
        return false;

    // Kraken invariant: at least one side present
    if (!has_asks && !has_bids) {
        WK_DEBUG("[PARSER] Both sides 'asks' and 'bids' missing in book message -> ignore message.");
        return false;
    }

    // checksum (required)
    std::uint64_t checksum = 0;
    if (!helper::parse_uint64_required(book, "checksum", checksum)) {
        WK_DEBUG("[PARSER] Field 'checksum' missing or invalid in book message -> ignore message.");
        return false;
    }
    out.checksum = static_cast<std::uint32_t>(checksum);

    // timestamp (Update only)
    if constexpr (requires { out.timestamp; }) {
        if (!adapter::parse_timestamp_required(book, "timestamp", out.timestamp)) {
            WK_DEBUG("[PARSER] Field 'timestamp' missing or invalid in book message -> ignore message.");
            return false;
        }
    }

    return true;
}

} // namespace wirekrak::protocol::kraken::parser::detail

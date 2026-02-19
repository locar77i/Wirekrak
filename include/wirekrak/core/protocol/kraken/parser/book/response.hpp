#pragma once

#include "wirekrak/core/protocol/kraken/schema/book/response.hpp"
#include "wirekrak/core/protocol/kraken/enums/payload_type.hpp"
#include "wirekrak/core/protocol/kraken/parser/result.hpp"
#include "wirekrak/core/protocol/kraken/parser/helpers.hpp"
#include "wirekrak/core/protocol/kraken/parser/adapters.hpp"
#include "wirekrak/core/protocol/kraken/parser/book/detail/parse_side_levels_common.hpp"


namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace parser {
namespace book {

struct response {
    [[nodiscard]]
    static inline Result parse(const simdjson::dom::element& root, schema::book::Book& out) noexcept {
        using namespace simdjson;

        // data array (required, exactly one element)
        simdjson::dom::array data;
        auto r = helper::parse_array_required(root, "data", data);
        if (r != Result::Parsed) {
            WK_DEBUG("[PARSER] Field 'data' missing or invalid in book message -> ignore message.");
            return r;
        }

        // enforce array size (exactly one element)
        if (data.size() != 1) {
            WK_DEBUG("[PARSER] Field 'data' does not contain exactly one element in book message -> ignore message.");
            return Result::InvalidSchema;
        }

        simdjson::dom::object book;
        if (data.at(0).get(book)) {
            WK_DEBUG("[PARSER] Field 'data[0]' invalid in book message -> ignore message.");
            return Result::InvalidSchema;
        }

        // symbol (required)
        r = adapter::parse_symbol_required(book, "symbol", out.symbol);
        if (r != Result::Parsed) {
            WK_DEBUG("[PARSER] Field 'symbol' missing in book message -> ignore message.");
            return r;
        }

        // sides (asks / bids)
        bool has_asks = false;
        r = detail::parse_side_levels_common(book, "asks", out.asks, has_asks);
        if (r != Result::Parsed) {
            return r;
        }

        bool has_bids = false;
        r = detail::parse_side_levels_common(book, "bids", out.bids, has_bids);
        if (r != Result::Parsed) {
            return r;
        }

        // Kraken invariant: at least one side present
        if (!has_asks && !has_bids) {
            WK_DEBUG("[PARSER] Both sides 'asks' and 'bids' missing in book message -> ignore message.");
            return Result::InvalidSchema;
        }

        // checksum (required)
        std::uint64_t checksum = 0;
        r = helper::parse_uint64_required(book, "checksum", checksum);
        if (r != Result::Parsed) {
            WK_DEBUG("[PARSER] Field 'checksum' missing or invalid in book message -> ignore message.");
            return r;
        }
        out.checksum = static_cast<std::uint32_t>(checksum);

        // timestamp (optional)
        r = adapter::parse_timestamp_optional(book, "timestamp", out.timestamp);
        if (r != Result::Parsed) {
            WK_DEBUG("[PARSER] Field 'timestamp' missing or invalid in book message -> ignore message.");
            return r;
        }

        return Result::Parsed;
    }


    [[nodiscard]]
    static inline Result parse(const simdjson::dom::element& root, schema::book::Response& out) noexcept {
        out = schema::book::Response{};
        using namespace simdjson;

        // Root
        auto r = helper::require_object(root);
        if (r != Result::Parsed) {
            WK_DEBUG("[PARSER] Root not an object in book message -> ignore message.");
            return r;
        }

        // type (required): snapshot | update
        kraken::PayloadType type;
        r = adapter::parse_payload_type_required(root, "type", type);
        if (r != Result::Parsed) {
            WK_DEBUG("[PARSER] Field 'type' invalid or missing in trade response -> ignore message.");
            return r;
        }
        out.type = type;

        return parse(root, out.book);
    }
};

} // namespace book
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core


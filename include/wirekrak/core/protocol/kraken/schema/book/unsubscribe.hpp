#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cassert>

#include "wirekrak/core/protocol/kraken/request/validate.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/common.hpp"
#include "wirekrak/core/symbol.hpp"
#include "lcr/json.hpp"
#include "lcr/optional.hpp"


namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace schema {
namespace book {

// ===============================================
// BOOK UNSUBSCRIBE REQUEST
// ===============================================
//
// Kraken WebSocket v2
//
// method:  "unsubscribe"
// channel: "book"
//
// ===============================================

struct Unsubscribe {
    using unsubscribe_tag = void;

    std::vector<Symbol> symbols;
    lcr::optional<std::uint32_t> depth{};
    lcr::optional<std::uint64_t> req_id{};

    [[nodiscard]]
    std::string to_json() const {
#ifndef NDEBUG
        request::validate_symbols(symbols);
        request::validate_req_id(req_id);
        // Optional field validation
        if (depth.has()) {
            assert(book::is_valid_depth(depth.value()) && "Invalid Kraken book depth value");
        }
#endif
        std::string j;
        j.reserve(256);

        j += "{\"method\":\"unsubscribe\",\"params\":{";
        j += "\"channel\":\"book\",";

        // -------------------------------------------
        // symbols array (required)
        // -------------------------------------------
        j += "\"symbol\":[";
        for (size_t i = 0; i < symbols.size(); ++i) {
            j += "\"";
            j += lcr::json::escape(symbols[i]);
            j += "\"";
            if (i + 1 < symbols.size())
                j += ",";
        }
        j += "]";

        // -------------------------------------------
        // depth (optional)
        // -------------------------------------------
        if (depth.has()) {
            j += ",\"depth\":";
            lcr::json::append(j, depth.value());
        }

        j += "}"; // close params

        // -------------------------------------------
        // req_id (optional)
        // -------------------------------------------
        if (req_id.has()) {
            j += ",\"req_id\":";
            lcr::json::append(j, req_id.value());
        }

        j += "}"; // close json

        return j;
    }
};

} // namespace book
} // namespace schema
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core

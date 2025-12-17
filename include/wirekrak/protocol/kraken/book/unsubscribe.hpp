#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cassert>

#include "wirekrak/protocol/kraken/book/common.hpp"
#include "wirekrak/core/symbol.hpp"
#include "lcr/json.hpp"
#include "lcr/optional.hpp"


namespace wirekrak {
namespace protocol {
namespace kraken {
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
    std::vector<Symbol> symbols;

    lcr::optional<std::uint32_t> depth{};
    lcr::optional<std::uint64_t> req_id{};

    [[nodiscard]]
    std::string to_json() const {
#ifndef NDEBUG
        // -------------------------------------------
        // Required invariants
        // -------------------------------------------
        assert(!symbols.empty() && "book::Unsubscribe requires at least one symbol");

        for (const auto& s : symbols) {
            assert(!std::string_view(s).empty() && "book::Unsubscribe symbol cannot be empty");
        }

        // -------------------------------------------
        // Optional field validation
        // -------------------------------------------
        if (depth.has()) {
            assert(book::is_valid_depth(depth.value()) && "Invalid Kraken book depth value");
        }

        if (req_id.has()) {
            assert(req_id.value() != 0 && "req_id should be non-zero");
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
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

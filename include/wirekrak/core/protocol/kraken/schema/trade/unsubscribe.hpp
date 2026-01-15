#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "wirekrak/core/protocol/kraken/request/validate.hpp"
#include "wirekrak/core/symbol.hpp"
#include "lcr/json.hpp"
#include "lcr/optional.hpp"


namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace schema {
namespace trade {

struct Unsubscribe {
    using unsubscribe_tag = void;

    std::vector<Symbol> symbols;
    lcr::optional<std::uint64_t> req_id{};

    std::string to_json() const {
#ifndef NDEBUG
        request::validate_symbols(symbols);
        request::validate_req_id(req_id);
#endif
        std::string j;
        j.reserve(256);

        j += "{\"method\":\"unsubscribe\",\"params\":{";
        j += "\"channel\":\"trade\",";

        // --- symbols array ---
        j += "\"symbol\":[";
        for (size_t i = 0; i < symbols.size(); ++i) {
            j += "\"";
            j += lcr::json::escape(symbols[i]);
            j += "\"";
            if (i + 1 < symbols.size()) j += ",";
        }
        j += "]";

        j += "}"; // close params

        // --- req_id (optional) ---
        if (req_id.has()) {
            j += ",\"req_id\":";
            lcr::json::append(j, req_id.value());
        }

        j += "}"; // close entire JSON object

        return j;
    }
};

} // namespace trade
} // namespace schema
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core

#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "wirekrak/core/symbol.hpp"
#include "lcr/json.hpp"
#include "lcr/optional.hpp"


namespace wirekrak{ 
namespace protocol {
namespace kraken {
namespace trade {


struct Subscribe {
    std::vector<Symbol> symbols;
    lcr::optional<bool> snapshot{};
    lcr::optional<std::uint64_t> req_id{};

    std::string to_json() const {
        std::string j;
        j.reserve(256);

        j += "{\"method\":\"subscribe\",\"params\":{";
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

        // --- snapshot ---
        if (snapshot.has()) {
            j += ",\"snapshot\":";
            j += (snapshot.value() ? "true" : "false");
        }

        j += "}"; // close params

        // --- req_id ---
         if (req_id.has()) {
            j += ",\"req_id\":";
            lcr::json::append(j, req_id.value());   // <---- FAST
        }

        j += "}"; // close json

        return j;
    }
};

} // namespace trade
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

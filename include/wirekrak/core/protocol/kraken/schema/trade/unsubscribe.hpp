#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "wirekrak/core/protocol/kraken/request/validate.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"
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
    lcr::optional<ctrl::req_id_t> req_id{};

    // ---------------------------------------------------------------------
    // Runtime maximum JSON size computation
    // ---------------------------------------------------------------------
    [[nodiscard]]
    inline std::size_t max_json_size() const noexcept {
        // Base structure:
        // {"method":"unsubscribe","params":{"channel":"trade","symbol":[]}}
        std::size_t size = 88;

        // Symbols (worst-case escaping expansion 6x)
        for (const auto& s : symbols) {
            size += 2;               // quotes
            size += 6 * s.size();    // worst-case escape
            size += 1;               // comma
        }

        // req_id
        if (req_id.has()) {
            size += 24; // ,"req_id":18446744073709551615
        }

        return size + 4; // closing braces safety
    }

    // ---------------------------------------------------------------------
    // Allocation-free JSON writer
    // ---------------------------------------------------------------------
    [[nodiscard]]
    inline std::size_t write_json(char* buffer) const noexcept {
#ifndef NDEBUG
        request::validate_symbols(symbols);
        request::validate_req_id(req_id);
#endif

        std::size_t pos = 0;

        // Prefix
        static constexpr char prefix[] =
            "{\"method\":\"unsubscribe\",\"params\":{"
            "\"channel\":\"trade\","
            "\"symbol\":[";
        std::memcpy(buffer + pos, prefix, sizeof(prefix) - 1);
        pos += sizeof(prefix) - 1;

        // Symbols
        for (std::size_t i = 0; i < symbols.size(); ++i) {
            buffer[pos++] = '"';
            pos += lcr::json::escape(buffer + pos, symbols[i]);
            buffer[pos++] = '"';

            if (i + 1 < symbols.size())
                buffer[pos++] = ',';
        }

        buffer[pos++] = ']';
        buffer[pos++] = '}'; // close params

        // req_id
        if (req_id.has()) {
            static constexpr char req_prefix[] = ",\"req_id\":";
            std::memcpy(buffer + pos, req_prefix, sizeof(req_prefix) - 1);
            pos += sizeof(req_prefix) - 1;
            pos += lcr::json::append(buffer + pos, req_id.value());
        }

        buffer[pos++] = '}'; // close entire JSON

        return pos;
    }

#ifndef WIREKRAK_NO_ALLOCATIONS
    // Convenience method (allocating) for tests / logging.
    std::string to_json() const {
        char buffer[4096]; // more than enough for any realistic subscription request
        std::size_t size = write_json(buffer);
        return std::string(buffer, size);
    }
#endif // WIREKRAK_NO_ALLOCATIONS

};

} // namespace trade
} // namespace schema
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core

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


struct Subscribe {
    using subscribe_tag = void;

    std::vector<Symbol> symbols;
    lcr::optional<bool> snapshot{};
    lcr::optional<ctrl::req_id_t> req_id{};

    // Runtime-computed worst-case size
    [[nodiscard]]
    inline std::size_t max_json_size() const noexcept {
        std::size_t size = 0;

        // Base structure
        size += sizeof("{\"method\":\"subscribe\",\"params\":{\"channel\":\"trade\",\"symbol\":[") - 1;
        size += sizeof("]") - 1;
        size += sizeof("}") - 1; // close params
        size += sizeof("}") - 1; // close json

        // Symbols
        for (const auto& s : symbols) {
            size += 2; // quotes
            size += 6 * s.size(); // worst-case escape expansion
            size += 1; // comma
        }

        if (!symbols.empty())
            size -= 1; // remove last comma

        // snapshot
        if (snapshot.has()) {
            size += sizeof(",\"snapshot\":true") - 1;
        }

        // req_id
        if (req_id.has()) {
            size += sizeof(",\"req_id\":") - 1;
            size += 20; // max uint64 digits
        }

        return size;
    }


    [[nodiscard]]
    inline std::size_t write_json(char* buffer) const noexcept {
#ifndef NDEBUG
        request::validate_symbols(symbols);
        request::validate_req_id(req_id);
#endif

        std::size_t pos = 0;

        auto append_literal = [&](const char* s, std::size_t len) {
            std::memcpy(buffer + pos, s, len);
            pos += len;
        };

        // {"method":"subscribe","params":{
        static constexpr char prefix[] =
            "{\"method\":\"subscribe\",\"params\":{";
        append_literal(prefix, sizeof(prefix) - 1);

        // "channel":"trade",
        static constexpr char channel[] =
            "\"channel\":\"trade\",";
        append_literal(channel, sizeof(channel) - 1);

        // "symbol":[
        static constexpr char symbol_prefix[] =
            "\"symbol\":[";
        append_literal(symbol_prefix, sizeof(symbol_prefix) - 1);

        // symbols
        for (std::size_t i = 0; i < symbols.size(); ++i) {
            buffer[pos++] = '"';
            pos += lcr::json::escape(buffer + pos, symbols[i]);
            buffer[pos++] = '"';

            if (i + 1 < symbols.size())
                buffer[pos++] = ',';
        }

        buffer[pos++] = ']';

        // snapshot
        if (snapshot.has()) {
            static constexpr char snap_prefix[] = ",\"snapshot\":";
            append_literal(snap_prefix, sizeof(snap_prefix) - 1);

            if (snapshot.value())
                append_literal("true", 4);
            else
                append_literal("false", 5);
        }

        buffer[pos++] = '}'; // close params

        // req_id
        if (req_id.has()) {
            static constexpr char req_prefix[] = ",\"req_id\":";
            append_literal(req_prefix, sizeof(req_prefix) - 1);
            pos += lcr::json::append(buffer + pos, req_id.value());
        }

        buffer[pos++] = '}';

#ifndef NDEBUG
        assert(pos <= max_json_size());
#endif

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

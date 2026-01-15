#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <ostream>
#include <sstream>

#include "wirekrak/core/protocol/kraken/enums/payload_type.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/common.hpp"
#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/timestamp.hpp"
#include "lcr/optional.hpp"


namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace schema {
namespace book {

// ===============================================
// BOOK MESSAGE
// ===============================================
//
// Represents an incremental order book message sent
// by Kraken WebSocket API ("update"/"snapshot").
//
// ===============================================

// -----------------------------------------------
// BOOK PAYLOAD
// -----------------------------------------------
struct Book {
    Symbol symbol;

    std::vector<Level> asks;
    std::vector<Level> bids;

    std::uint32_t checksum;
    lcr::optional<Timestamp> timestamp;

    // ---------------------------------------------------------
    // Debug / diagnostic dump
    // ---------------------------------------------------------
    inline void dump(std::ostream& os) const {
        auto dump_levels = [&os](const std::vector<Level>& levels) {
            os << "[";
            for (std::size_t i = 0; i < levels.size(); ++i) {
                const auto& lvl = levels[i];
                os << "{"
                << "\"price\":" << lvl.price << ","
                << "\"qty\":" << lvl.qty
                << "}";
                if (i + 1 < levels.size()) {
                    os << ",";
                }
            }
            os << "]";
        };

        os << "{"
        << "\"symbol\":\"" << symbol << "\","
        << "\"checksum\":" << checksum;

        if (timestamp.has()) {
            os << ",\"timestamp\":\""
            << wirekrak::core::to_string(timestamp.value())
            << "\"";
        }

        os << ",\"asks\":";
        dump_levels(asks);

        os << ",\"bids\":";
        dump_levels(bids);

        os << "}";
    }

#ifndef NDEBUG
    // ---------------------------------------------------------
    // String helper (debug / logging)
    // NOTE: Allocates. Intended for debugging/logging only.
    // ---------------------------------------------------------
    [[nodiscard]]
    inline std::string str() const {
        std::ostringstream oss;
        dump(oss);
        return oss.str();
    }
#endif
};

// Stream operator<< delegates to dump(); allocation-free.
inline std::ostream& operator<<(std::ostream& os, const Book& u) {
    u.dump(os);
    return os;
}


// ===============================================
// BOOK RESPONSE (snapshot or update)
// ===============================================
struct Response {
    PayloadType type;
    Book book;

    [[nodiscard]]
    inline Symbol get_symbol() const noexcept{
        return book.symbol;
    }

    // ---------------------------------------------------------
    // Dump
    // ---------------------------------------------------------
    inline void dump(std::ostream& os) const {
        os << "[BOOK RESPONSE] {"
           << "type=" << to_string(type)
           << ", book=";
        book.dump(os);
        os << "}";
    }

#ifndef NDEBUG
    // ---------------------------------------------------------
    // String helper (debug / logging)
    // NOTE: Allocates. Intended for debugging/logging only.
    // ---------------------------------------------------------
    inline std::string str() const {
        std::ostringstream oss;
        dump(oss);
        return oss.str();
    }
#endif
};

// Stream operator<< delegates to dump(); allocation-free.
inline std::ostream& operator<<(std::ostream& os, const Response& r) {
    r.dump(os);
    return os;
}

} // namespace book
} // namespace schema
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core

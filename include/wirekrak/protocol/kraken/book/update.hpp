#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <ostream>
#include <sstream>

#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/timestamp.hpp"
#include "wirekrak/protocol/kraken/book/common.hpp"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace book {

// ===============================================
// BOOK UPDATE MESSAGE
// ===============================================
//
// Represents an incremental order book update sent
// by Kraken WebSocket API (type = "update").
//
// This message updates price levels since the last
// snapshot or update.
//
// ===============================================

// -----------------------------------------------
// BOOK UPDATE PAYLOAD
// -----------------------------------------------
struct Update {
    Symbol symbol;

    std::vector<Level> asks;
    std::vector<Level> bids;

    std::uint32_t checksum;
    Timestamp timestamp;

    // ---------------------------------------------------------
    // Debug / diagnostic dump
    // ---------------------------------------------------------
    inline void dump(std::ostream& os) const {
        os << "[BOOK UPDATE] {"
           << "symbol=" << symbol
           << ", ts=" << timestamp
           << ", checksum=" << checksum
           << "}\n";

        if (!asks.empty()) {
            os << "    asks[" << asks.size() << "]: ";
            for (const auto& l : asks) {
                os << "(price=" << l.price << ", qty=" << l.qty << ") ";
            }
            os << '\n';
        }

        if (!bids.empty()) {
            os << "    bids[" << bids.size() << "]: ";
            for (const auto& l : bids) {
                os << "(price=" << l.price << ", qty=" << l.qty << ") ";
            }
            os << '\n';
        }
    }

    // ---------------------------------------------------------
    // String helper (debug / logging)
    // ---------------------------------------------------------
    [[nodiscard]]
    inline std::string str() const {
        std::ostringstream oss;
        dump(oss);
        return oss.str();
    }
};

// Stream operator
inline std::ostream& operator<<(std::ostream& os, const Update& u) {
    u.dump(os);
    return os;
}

} // namespace book
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

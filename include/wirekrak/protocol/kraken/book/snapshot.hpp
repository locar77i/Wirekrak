#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <ostream>
#include <sstream>

#include "wirekrak/core/symbol.hpp"
#include "wirekrak/protocol/kraken/book/common.hpp"

namespace wirekrak {
namespace protocol {
namespace kraken {
namespace book {

// ===============================================
// BOOK SNAPSHOT MESSAGE
// ===============================================
//
// Represents a full order book snapshot sent by
// Kraken WebSocket API (type = "snapshot").
//
// This message contains the initial state of the
// order book at subscription time.
//
// ===============================================

// -----------------------------------------------
// BOOK SNAPSHOT PAYLOAD
// -----------------------------------------------
struct Snapshot {
    Symbol symbol;

    std::vector<Level> asks;
    std::vector<Level> bids;

    std::uint32_t checksum;

    // ------------------------------------------------------------
    // Debug / diagnostic dump
    // ------------------------------------------------------------
    inline void dump(std::ostream& os) const {
        os << "[BOOK SNAPSHOT] {symbol=" << symbol
           << ", asks=" << asks.size()
           << ", bids=" << bids.size()
           << ", checksum=" << checksum
           << "}";
    }

    // ------------------------------------------------------------
    // String helper
    // ------------------------------------------------------------
    inline std::string str() const {
        std::ostringstream oss;
        dump(oss);
        return oss.str();
    }
};

// Stream operator
inline std::ostream& operator<<(std::ostream& os, const Snapshot& s) {
    s.dump(os);
    return os;
}

} // namespace book
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

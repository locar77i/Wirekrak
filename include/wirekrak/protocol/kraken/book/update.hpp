#pragma once

#include <vector>
#include <cstdint>

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
};

} // namespace book
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

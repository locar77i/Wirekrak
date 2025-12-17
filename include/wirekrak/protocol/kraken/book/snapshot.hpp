#pragma once

#include <vector>
#include <cstdint>

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
};

} // namespace book
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

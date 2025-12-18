#pragma once

#include <cstdint>
#include <vector>

#include "wirekrak/protocol/kraken/enums/side.hpp"
#include "wirekrak/protocol/kraken/enums/order_type.hpp"
#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/timestamp.hpp"
#include "lcr/optional.hpp"

namespace wirekrak::protocol::kraken::trade {

// ===============================================
// TRADE EVENT (single element in data[])
// ===============================================
struct Trade {
    std::uint64_t trade_id;
    Symbol        symbol;
    double        price;
    double        qty;
    Side          side;
    Timestamp     timestamp;
    lcr::optional<OrderType> ord_type;
};

// ===============================================
// TRADE MESSAGE TYPE
// ===============================================
enum class Type : uint8_t {
    Snapshot,
    Update
};

// ===============================================
// TRADE RESPONSE (snapshot or update)
// ===============================================
struct Response {
    Type type;
    std::vector<Trade> trades;
};

} // namespace wirekrak::protocol::kraken::trade

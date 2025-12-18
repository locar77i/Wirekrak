#pragma once

#include <string>
#include <cstdint>

#include "wirekrak/protocol/kraken/enums/side.hpp"
#include "wirekrak/protocol/kraken/enums/order_type.hpp"
#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/timestamp.hpp"
#include "lcr/optional.hpp"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace trade {

struct Response {
    std::uint64_t trade_id;
    Symbol symbol;
    double price;
    double qty;
    Side side;
    Timestamp timestamp;
    lcr::optional<OrderType> ord_type;
};

} // namespace trade
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

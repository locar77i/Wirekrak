#pragma once

#include <string>
#include <cstdint>

#include "wirekrak/core/types.hpp"
#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/timestamp.hpp"
#include "lcr/optional.hpp"


namespace wirekrak {
namespace schema {
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
} // namespace schema
} // namespace wirekrak

#include "wirekrak/lite/dto/trade.hpp"

#include <ostream>


namespace wirekrak::lite::dto {

// ---------------------------------
// Inline helpers (defined out-of-line)
// ---------------------------------

bool trade::is_buy() const noexcept {
    return taker_side == side::buy;
}

bool trade::is_sell() const noexcept {
    return taker_side == side::sell;
}

// ---------------------------------
// Debug / logging helper
// ---------------------------------

std::ostream& operator<<(std::ostream& os, const trade& t) {
    os << "[trade] {"
       << "id=" << t.trade_id
       << ", symbol=" << t.symbol
       << ", price=" << t.price
       << ", qty=" << t.quantity
       << ", side=" << to_string(t.taker_side)
       << ", ts_ns=" << t.timestamp_ns;

    if (t.order_type) {
        os << ", ord_type=" << *t.order_type;
    }

    os << "}";

    return os;
}

} // namespace wirekrak::lite::dto

#include "wirekrak/lite/domain/trade.hpp"

#include <ostream>


namespace wirekrak::lite::domain {

// ---------------------------------
// Inline helpers (defined out-of-line)
// ---------------------------------

bool Trade::is_buy() const noexcept {
    return taker_side == Side::Buy;
}

bool Trade::is_sell() const noexcept {
    return taker_side == Side::Sell;
}

// ---------------------------------
// Debug / logging helper
// ---------------------------------

std::ostream& operator<<(std::ostream& os, const Trade& t) {
    os << "[Trade] {"
       << "id=" << t.trade_id
       << ", symbol=" << t.symbol
       << ", price=" << t.price
       << ", qty=" << t.quantity
       << ", side=" << to_string(t.taker_side)
       << ", ts_ns=" << t.timestamp_ns;

    if (t.order_type) {
        os << ", ord_type=" << *t.order_type;
    }
    os << ", tag=" << to_string(t.tag)
       << "}";

    return os;
}

} // namespace wirekrak::lite::domain

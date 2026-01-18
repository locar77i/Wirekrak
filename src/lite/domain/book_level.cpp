#include "wirekrak/lite/domain/book_level.hpp"

#include <ostream>

namespace wirekrak::lite::domain {

// ---------------------------------
// Inline helpers
// ---------------------------------

bool BookLevel::is_bid() const noexcept {
    return book_side == Side::Buy;
}

bool BookLevel::is_ask() const noexcept {
    return book_side == Side::Sell;
}

// ---------------------------------
// Debug / logging helper
// ---------------------------------

std::ostream& operator<<(std::ostream& os, const BookLevel& lvl) {
    os << "[BookLevel] {"
       << "symbol=" << lvl.symbol
       << ", side=" << to_string(lvl.book_side)
       << ", price=" << lvl.price
       << ", qty=" << lvl.quantity;

    if (lvl.timestamp_ns) {
        os << ", ts_ns=" << *lvl.timestamp_ns;
    }

    os << ", tag=" << to_string(lvl.tag)
       << "}";

    return os;
}

} // namespace wirekrak::lite::domain

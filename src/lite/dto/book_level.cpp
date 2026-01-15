#include "wirekrak/lite/dto/book_level.hpp"

#include <ostream>

namespace wirekrak::lite::dto {

// ---------------------------------
// Inline helpers
// ---------------------------------

bool book_level::is_bid() const noexcept {
    return book_side == side::buy;
}

bool book_level::is_ask() const noexcept {
    return book_side == side::sell;
}

// ---------------------------------
// Debug / logging helper
// ---------------------------------

std::ostream& operator<<(std::ostream& os, const book_level& lvl) {
    os << "[book_level] {"
       << "symbol=" << lvl.symbol
       << ", side=" << to_string(lvl.book_side)
       << ", price=" << lvl.price
       << ", qty=" << lvl.quantity;

    if (lvl.timestamp_ns) {
        os << ", ts_ns=" << *lvl.timestamp_ns;
    }

    os << ", origin=" << to_string(lvl.origin)
       << "}";

    return os;
}

} // namespace wirekrak::lite::dto

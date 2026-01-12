#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <ostream>
#include <sstream>

#include "wirekrak/protocol/kraken/enums/side.hpp"
#include "wirekrak/protocol/kraken/enums/order_type.hpp"
#include "wirekrak/protocol/kraken/enums/payload_type.hpp"
#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/timestamp.hpp"
#include "lcr/optional.hpp"

namespace wirekrak {
namespace protocol {
namespace kraken {
namespace schema {
namespace trade {

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

    [[nodiscard]]
    inline Symbol get_symbol() const noexcept{
        return symbol;
    }

    // ---------------------------------------------------------
    // Dump (no allocations)
    // ---------------------------------------------------------
    inline void dump(std::ostream& os) const {
        os << "[TRADE] {"
           << "id=" << trade_id
           << ", symbol=" << symbol
           << ", price=" << price
           << ", qty=" << qty
           << ", side=" << to_string(side)
           << ", timestamp=" << wirekrak::to_string(timestamp);

        if (ord_type.has()) {
            os << ", ord_type=" << to_string(ord_type.value());
        }

        os << "}";
    }

#ifndef NDEBUG
    // ---------------------------------------------------------
    // String helper (debug / logging)
    // NOTE: Allocates. Intended for debugging/logging only.
    // ---------------------------------------------------------
    inline std::string str() const {
        std::ostringstream oss;
        dump(oss);
        return oss.str();
    }
#endif
};

// Stream operator<< delegates to dump(); allocation-free.
inline std::ostream& operator<<(std::ostream& os, const Trade& t) {
    t.dump(os);
    return os;
}


// ===============================================
// TRADE RESPONSE (snapshot or update)
// ===============================================
struct Response {
    PayloadType type;
    std::vector<Trade> trades;

    // ---------------------------------------------------------
    // Dump
    // ---------------------------------------------------------
    inline void dump(std::ostream& os) const {
        os << "[TRADE RESPONSE] {"
           << "type=" << to_string(type)
           << ", trades=[";

        for (std::size_t i = 0; i < trades.size(); ++i) {
            trades[i].dump(os);
            if (i + 1 < trades.size()) {
                os << ", ";
            }
        }

        os << "]}";
    }

#ifndef NDEBUG
    // ---------------------------------------------------------
    // String helper (debug / logging)
    // NOTE: Allocates. Intended for debugging/logging only.
    // ---------------------------------------------------------
    [[nodiscard]]
    inline std::string str() const {
        std::ostringstream oss;
        dump(oss);
        return oss.str();
    }
#endif
};

// Stream operator<< delegates to dump(); allocation-free.
inline std::ostream& operator<<(std::ostream& os, const Response& r) {
    r.dump(os);
    return os;
}

} // namespace trade
} // namespace schema
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

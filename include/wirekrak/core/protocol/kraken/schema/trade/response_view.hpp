#pragma once

#include <span>
#include <ostream>
#include <sstream>
#include <string>

#include "wirekrak/core/protocol/kraken/enums/payload_type.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/response.hpp"


namespace wirekrak::core::protocol::kraken::schema::trade {

/*
===============================================================================
Trade ResponseView (symbol-scoped)
===============================================================================

ResponseView is a zero-copy, symbol-scoped projection of a Kraken trade response.

It preserves protocol semantics (snapshot vs update) while allowing efficient
per-symbol routing without duplicating or flattening payloads in Core.

ResponseView does NOT own data.
Its lifetime is bounded by the dispatch call that delivers it.

===============================================================================
*/
struct ResponseView {
    Symbol symbol;                            // Routing key (explicit)
    PayloadType type;                         // Snapshot or Update
    std::span<const Trade* const> trades;     // Trades for exactly one symbol

    [[nodiscard]]
    inline constexpr Symbol get_symbol() const noexcept {
        return symbol;
    }

    [[nodiscard]]
    inline constexpr bool is_snapshot() const noexcept {
        return type == PayloadType::Snapshot;
    }

    [[nodiscard]]
    inline constexpr bool is_update() const noexcept {
        return type == PayloadType::Update;
    }

    // ---------------------------------------------------------
    // Dump (no allocations)
    // ---------------------------------------------------------
    inline void dump(std::ostream& os) const {
        os << "[TRADE RESPONSE VIEW] {"
           << "symbol=" << symbol
           << ", type=" << to_string(type)
           << ", trades=[";

        for (std::size_t i = 0; i < trades.size(); ++i) {
            trades[i]->dump(os);
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
inline std::ostream& operator<<(std::ostream& os, const ResponseView& r) {
    r.dump(os);
    return os;
}

} // namespace wirekrak::core::protocol::kraken::schema::trade

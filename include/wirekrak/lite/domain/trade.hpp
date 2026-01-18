// NOTE: This header defines a public domain type.
// Users should include <wirekrak/lite.hpp> instead of this file directly.

#pragma once

#include <cstdint>
#include <string>
#include <optional>

#include "wirekrak/lite/enums.hpp"


namespace wirekrak::lite::domain {

// -----------------------------
// Trade DTO (API surface)
// -----------------------------
struct Trade {
    std::uint64_t trade_id;
    std::string   symbol;
    double        price;
    double        quantity;
    Side          taker_side;
    std::uint64_t timestamp_ns;
    std::optional<std::string> order_type;
    Tag           tag;

    // Minimal, inline helpers are OK
    [[nodiscard]] bool is_buy() const noexcept;
    [[nodiscard]] bool is_sell() const noexcept;
};


std::ostream& operator<<(std::ostream&, const Trade&);

} // namespace wirekrak::lite::domain

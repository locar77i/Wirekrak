#pragma once

#include <cstdint>
#include <string>
#include <optional>

#include "wirekrak/lite/enums.hpp"


namespace wirekrak::lite::dto {

// -----------------------------
// Trade DTO (API surface)
// -----------------------------
struct trade {
    std::uint64_t trade_id;
    std::string   symbol;
    double        price;
    double        quantity;
    side          taker_side;
    std::uint64_t timestamp_ns;
    std::optional<std::string> order_type;
    origin        origin;

    // Minimal, inline helpers are OK
    [[nodiscard]] bool is_buy() const noexcept;
    [[nodiscard]] bool is_sell() const noexcept;
};


std::ostream& operator<<(std::ostream&, const trade&);

} // namespace wirekrak::lite::dto

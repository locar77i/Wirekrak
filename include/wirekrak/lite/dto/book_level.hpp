#pragma once

#include <string>
#include <cstdint>
#include <optional>

#include "wirekrak/lite/enums.hpp"


namespace wirekrak::lite::dto {

// -----------------------------
// Book level DTO (API surface)
// -----------------------------
struct book_level {
    std::string symbol;
    side        book_side;          // bid / ask
    double      price;
    double      quantity;
    std::optional<std::uint64_t> timestamp_ns; // present only for updates
    origin      origin;             // snapshot | update

    [[nodiscard]] bool is_bid() const noexcept;
    [[nodiscard]] bool is_ask() const noexcept;
    [[nodiscard]] bool has_timestamp() const noexcept {
        return timestamp_ns.has_value();
    }
};

std::ostream& operator<<(std::ostream&, const book_level&);

} // namespace wirekrak::lite::dto

// NOTE: This header defines a public domain type.
// Users should include <wirekrak/lite.hpp> instead of this file directly.

#pragma once

#include <string>
#include <cstdint>
#include <optional>

#include "wirekrak/lite/enums.hpp"


namespace wirekrak::lite::domain {

// -----------------------------
// Book level DTO (API surface)
// -----------------------------
struct BookLevel {
    std::string symbol;
    Side        book_side; // bid / ask
    double      price;
    double      quantity;
    std::optional<std::uint64_t> timestamp_ns; // present only for updates
    Tag         tag; // snapshot | update
    [[nodiscard]] bool is_bid() const noexcept;
    [[nodiscard]] bool is_ask() const noexcept;
    [[nodiscard]] bool has_timestamp() const noexcept {
        return timestamp_ns.has_value();
    }
};

std::ostream& operator<<(std::ostream&, const BookLevel&);

} // namespace wirekrak::lite::domain

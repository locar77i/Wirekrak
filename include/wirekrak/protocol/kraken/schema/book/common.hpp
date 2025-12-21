#pragma once

#include <cstdint>

namespace wirekrak {
namespace protocol {
namespace kraken {
namespace book {

// -----------------------------------------------
// PRICE LEVEL
// -----------------------------------------------
struct Level {
    double price;
    double qty;
};

// ===============================================
// BOOK DEPTH VALIDATION
// ===============================================
//
// Kraken allows only a fixed set of depth values
// for book subscriptions and acknowledgements.
//
// Valid values:
//   10, 25, 100, 500, 1000
//
// This helper is constexpr and zero-cost.
// ===============================================
[[nodiscard]]
constexpr inline bool is_valid_depth(std::uint32_t depth) noexcept {
    switch (depth) {
        case 10:
        case 25:
        case 100:
        case 500:
        case 1000:
            return true;
        default:
            return false;
    }
}

} // namespace book
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

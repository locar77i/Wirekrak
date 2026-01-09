#pragma once

#include <string_view>

namespace wirekrak::lite {

// -----------------------------
// Trade / Order side
// -----------------------------
enum class side {
    buy,
    sell
};

constexpr std::string_view to_string(side s) noexcept {
    switch (s) {
        case side::buy:  return "buy";
        case side::sell: return "sell";
    }
    return "unknown";
}

} // namespace wirekrak::lite

#pragma once

#include <cstdint>
#include <string_view>

namespace wirekrak::core::protocol::policy {

enum class Liveness : std::uint8_t {
    Passive,  // Liveness reflects observable protocol traffic only
    Active    // Protocol is responsible for maintaining liveness
};

[[nodiscard]]
inline constexpr std::string_view to_string(Liveness policy) noexcept {
    switch (policy) {
        case Liveness::Passive: return "Passive";
        case Liveness::Active:  return "Active";
        default:                return "Unknown";
    }
}

} // namespace wirekrak::core::protocol::policy

#pragma once

#include <cstdint>
#include <string>
#include <ostream>
#include <sstream>
#include <type_traits>

#include "flashstrike/types.hpp"

namespace flashstrike {
namespace matching_engine {
namespace conf {

// ============================================================================
//  NormalizedInstrument — Representation of a Normalized Asset Pair
// ============================================================================
//  A precomputed, cache-aligned, integer representation of a Instrument.
//  All floating-point units (e.g. price_tick_units = 0.01 USD) are converted
//  into scaled integer "ticks".
//  The MatchingEngine and OrderBook use this normalized structure internally.
// ----------------------------------------------------------------------------
struct alignas(64) NormalizedInstrument {
    // Accessors
    inline constexpr Price price_tick_size() const noexcept { return price_tick_size_; }
    inline constexpr Price price_min_scaled() const noexcept { return price_min_scaled_; }
    inline constexpr Price price_max_scaled() const noexcept { return price_max_scaled_; }
    inline constexpr Quantity qty_tick_size() const noexcept { return qty_tick_size_; }
    inline constexpr Quantity qty_min_scaled() const noexcept { return qty_min_scaled_; }
    inline constexpr Quantity qty_max_scaled() const noexcept { return qty_max_scaled_; }
    inline constexpr Notional min_notional() const noexcept { return min_notional_; }
    // Helpers
    inline constexpr void update_price_upper_limit(std::uint64_t num_ticks) noexcept {
        price_max_scaled_ = static_cast<Price>(num_ticks * price_tick_size_);
    }

    inline void debug_dump(std::ostream& os) const {
        os << "[NormalizedInstrument]:\n"
        << "  Price Tick Size: " << price_tick_size_ << "\n"
        << "  Price Min      : " << price_min_scaled_ << "\n"
        << "  Price Max      : " << price_max_scaled_ << "\n"
        << "  Qty Tick Size  : " << qty_tick_size_ << "\n"
        << "  Qty Min        : " << qty_min_scaled_ << "\n"
        << "  Qty Max        : " << qty_max_scaled_ << "\n"
        << "  Min Notional   : " << min_notional_ << "\n"
        ;
    }
    inline std::string to_string() const {
        std::ostringstream oss;
        debug_dump(oss);
        return oss.str();
    }

private:
    friend struct Instrument;
    // ---- Price domain ----
    Price     price_tick_size_{};    // scaled integer size of one price tick
    Price     price_min_scaled_{};   // usually = price_tick_size
    Price     price_max_scaled_{};   // scaled integer maximum price
    // ---- Quantity domain ----
    Quantity  qty_tick_size_{};      // scaled integer qty tick
    Quantity  qty_min_scaled_{};     // scaled integer min qty
    Quantity  qty_max_scaled_{};      // scaled integer max qty
    // ---- Notional domain ----
    Notional  min_notional_{};       // scaled integer min trade notional
};
// Structural safety
static_assert(sizeof(NormalizedInstrument) <= 64, "NormalizedInstrument must fit in one cache line");
static_assert(alignof(NormalizedInstrument) == 64, "NormalizedInstrument must be cache-line aligned");
static_assert(std::is_trivially_copyable_v<NormalizedInstrument>, "NormalizedInstrument must be trivially copyable");
static_assert(std::is_standard_layout_v<NormalizedInstrument>, "NormalizedInstrument must be standard layout");

} // namespace conf
} // namespace matching_engine
} // namespace flashstrike

#pragma once

#include <cstdint>
#include <string>
#include <ostream>
#include <sstream>
#include <type_traits>

#include "flashstrike/matching_engine/conf/normalized_instrument.hpp"
#include "lcr/normalization.hpp"


namespace flashstrike {
namespace matching_engine {
namespace conf {

// =============================================================================
//  Instrument — Representation of a Tradable Asset Pair
// =============================================================================
//
//  This structure is inspired by Kraken's REST API "Tradable Asset Pair" schema,
//  but trimmed down to include only the fields relevant for order book
//  construction, tick normalization, and partition planning.
//
//  It acts as the *semantic* layer sitting above the purely numerical
//  PartitionPlan (the mechanical layout).
//
// ----------------------------------------------------------------------------- 
//  Mapping to Kraken API fields
// -----------------------------------------------------------------------------
//
//  Kraken Field               | Instrument Field            | Notes
//  ---------------------------|-----------------------------|-------------------------------
//  base                       | base_symbol                 | e.g. "BTC"
//  quote                      | quote_symbol                | e.g. "USD"
//  pair_decimals              | price_decimals              | Price precision in decimals.
//  lot_decimals               | qty_decimals                | Quantity precision (base).
//  tick_size                  | price_tick_units            | Minimum valid price increment.
//  ordermin                   | min_qty_units               | Minimum order size (in base units).
//  costmin                    | min_notional_units          | Minimum notional cost (in quote).
//  status                     | status                      | Online / limit_only / post_only, etc.
//
//  Other Kraken fields like leverage, fees, margin_call, etc. are intentionally omitted.
//
// -----------------------------------------------------------------------------
//  Notes:
//  - This struct uses//units* (double) for human readability.
//  - Normalization to integer ticks happens during PartitionPlan computation.
//  - It defines a compact, engine-friendly abstraction for one tradable market.
// -----------------------------------------------------------------------------
struct Instrument {
    // --- Symbol identifiers ---
    char base_symbol[5];        // e.g. "BTC"
    char quote_symbol[5];       // e.g. "USD"
    char name[10];              // Instrument name (market symbol), e.g. "BTC/USD"
    // --- Tick and precision settings ---
    double price_tick_units;    // e.g. 0.01 USD
    double qty_tick_units;      // e.g. 0.0001 BTC
    uint8_t price_decimals;     // e.g. 2  (0.01)
    uint8_t qty_decimals;       // e.g. 4  (0.0001)
    // --- Bounds ---
    double price_max_units;     // e.g. 200'000.0 USD
    double qty_max_units;       // e.g. 100.0 BTC
    double min_qty_units;       // Minimum allowed base size
    double min_notional_units;  // Minimum trade notional value (quote terms)
    // --- Market metadata ---
    char status[16];            // "online", "limit_only", etc.

    // Normalization method
    inline NormalizedInstrument normalize(std::uint64_t num_ticks) const noexcept {
            NormalizedInstrument ni{};
            // --- Price domain ---
            const std::uint64_t price_scale = lcr::normalize_tick_size(price_tick_units, ni.price_tick_size_);
            ni.price_min_scaled_ = ni.price_tick_size_; // usually tick-sized min
            ni.price_max_scaled_ = static_cast<Price>(std::llround(price_max_units * price_scale));
            // --- Quantity domain ---
            int64_t qty_ts = 0;
            const std::uint64_t qty_scale = lcr::normalize_tick_size(qty_tick_units, qty_ts);
            ni.qty_tick_size_ = static_cast<Quantity>(qty_ts);
            ni.qty_min_scaled_ = static_cast<Quantity>(std::llround(min_qty_units * qty_scale));
            ni.qty_max_scaled_ = static_cast<Quantity>(std::llround(qty_max_units * qty_scale));
            // --- Notional domain (scaled in same integer basis) ---
            const long double notional_scaled =
                static_cast<long double>(min_notional_units)
                * static_cast<long double>(price_scale)
                * static_cast<long double>(qty_scale);
            ni.min_notional_ = static_cast<Notional>(std::llround(notional_scaled));
            // Update price max based on the partitioning num_ticks (must be greater than scaled max due to rounding)
            ni.price_max_scaled_ = static_cast<Price>(num_ticks * ni.price_tick_size_);
        #ifdef DEBUG
            assert(ni.price_tick_size_ > 0);
            assert(ni.price_min_scaled_ >= ni.price_tick_size_);
            assert(ni.qty_tick_size_ > 0);
            assert(ni.qty_max_scaled_ >= ni.qty_min_scaled_);
            assert(ni.min_notional_ > 0);
        #endif
            return ni;
        }

    inline std::string get_symbol(char separator = '/') const noexcept {
        return std::string(base_symbol) + separator + std::string(quote_symbol);
    }

    // ---------------------------------------------------------
    // Normalization helpers
    // ---------------------------------------------------------

    inline Price normalize_price(double user_price_units) const noexcept {
        if (user_price_units <= 0.0) {
            return 0;
        }
        const double ticks = user_price_units / price_tick_units;
        return static_cast<Price>(std::floor(ticks));
    }

    inline Quantity normalize_quantity(double user_qty_units) const noexcept {
        if (user_qty_units <= 0.0) {
            return 0;
        }
        const double ticks = user_qty_units / qty_tick_units;
        return static_cast<Quantity>(std::floor(ticks));
    }

    // ---------------------------------------------------------
    // Denormalization helpers
    // ---------------------------------------------------------
    inline double denormalize_price(Price price_ticks) const noexcept {
        return static_cast<double>(price_ticks) * price_tick_units;
    }

    inline double denormalize_quantity(Quantity qty_ticks) const noexcept {
        return static_cast<double>(qty_ticks) * qty_tick_units;
    }

    inline void debug_dump(std::ostream& os) const {
        os << "[Instrument]: " << name << "\n"
        << "  Base Symbol       : " << base_symbol << "\n"
        << "  Quote Symbol      : " << quote_symbol << "\n"
        << "  Price Tick Size   : " << price_tick_units << "\n"
        << "  Quantity Tick Size: " << qty_tick_units << "\n"
        << "  Price Decimals    : " << static_cast<int>(price_decimals) << "\n"
        << "  Quantity Decimals : " << static_cast<int>(qty_decimals) << "\n"
        << "  Max Price Units   : " << price_max_units << "\n"
        << "  Max Quantity Units: " << qty_max_units << "\n"
        << "  Min Quantity Units: " << min_qty_units << "\n"
        << "  Min Notional Units: " << min_notional_units << "\n"
        << "  Status: " << status << "\n"
        ;
    }
    inline std::string to_string() const {
        std::ostringstream oss;
        debug_dump(oss);
        return oss.str();
    }

};


} // namespace conf
} // namespace matching_engine
} // namespace flashstrike

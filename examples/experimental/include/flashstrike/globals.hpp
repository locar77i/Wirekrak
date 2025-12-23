#pragma once

#include "flashstrike/matching_engine/conf/instrument.hpp"


namespace flashstrike {

// =============================================================================
//  BTC/USD - Reference Asset Specification
// =============================================================================
inline constexpr matching_engine::conf::Instrument BTC_USD = {
    .base_symbol        = "BTC",
    .quote_symbol       = "USD",
    .name               = "BTC/USD",
    .price_tick_units   = 0.1,             // $0.10 per tick (fine but efficient)
    .qty_tick_units     = 0.0001,          // 0.0001 BTC precision
    .price_decimals     = 1,               // one decimal place for price
    .qty_decimals       = 4,               // four decimals for quantity
    .price_max_units    = 200'000.0,       // up to $200,000
    .qty_max_units      = 100.0,           // up to 100 BTC per order
    .min_qty_units      = 0.0001,          // minimum 0.0001 BTC
    .min_notional_units = 1.0,            // $1 minimum trade
    .status             = "online"
};


// =============================================================================
//  ETH/USD - Reference Asset Specification
// =============================================================================
inline constexpr matching_engine::conf::Instrument ETH_USD = {
    .base_symbol        = "ETH",
    .quote_symbol       = "USD",
    .name               = "ETH/USD",
    .price_tick_units   = 0.01,            // $0.01 per tick
    .qty_tick_units     = 0.001,           // 0.001 ETH precision
    .price_decimals     = 2,               // two decimals for price
    .qty_decimals       = 3,               // three decimals for quantity
    .price_max_units    = 10'000.0,        // up to $10,000
    .qty_max_units      = 1'000.0,         // up to 1 000 ETH per order
    .min_qty_units      = 0.001,           // minimum 0.001 ETH
    .min_notional_units = 1.0,            // $1 minimum trade
    .status             = "online"
};


// =============================================================================
//  SOL/USD - Reference Asset Specification
// =============================================================================
inline constexpr matching_engine::conf::Instrument SOL_USD = {
    .base_symbol        = "SOL",
    .quote_symbol       = "USD",
    .name               = "SOL/USD",
    .price_tick_units   = 0.01,            // $0.01 per tick (Kraken standard)
    .qty_tick_units     = 0.001,           // 0.001 SOL
    .price_decimals     = 2,               // 2 decimal places
    .qty_decimals       = 3,               // 3 decimal places
    .price_max_units    = 10'000.0,        // up to $10,000
    .qty_max_units      = 1'000.0,         // max 1 000 SOL per order
    .min_qty_units      = 0.001,           // minimum 0.001 SOL
    .min_notional_units = 1.0,             // $1 minimum trade
    .status             = "online"
};


// =============================================================================
//  LTC/USD - Reference Asset Specification
// =============================================================================
inline constexpr matching_engine::conf::Instrument LTC_USD = {
    .base_symbol        = "LTC",
    .quote_symbol       = "USD",
    .name               = "LTC/USD",
    .price_tick_units   = 0.01,            // $0.01 per tick (1-cent granularity)
    .qty_tick_units     = 0.001,           // 0.001 LTC increments
    .price_decimals     = 2,               // 2 decimal places for price
    .qty_decimals       = 3,               // 3 decimals for quantity
    .price_max_units    = 5'000.0,         // up to $5,000
    .qty_max_units      = 1'000.0,         // 1 000 LTC max per order
    .min_qty_units      = 0.001,           // minimum 0.001 LTC
    .min_notional_units = 1.0,             // $1 minimum trade value
    .status             = "online"
};


// =============================================================================
//  XRP/USD - Reference Asset Specification
// =============================================================================
inline constexpr matching_engine::conf::Instrument XRP_USD = {
    .base_symbol        = "XRP",
    .quote_symbol       = "USD",
    .name               = "XRP/USD",
    .price_tick_units   = 0.0001,          // $0.0001 per tick
    .qty_tick_units     = 1.0,             // 1 XRP
    .price_decimals     = 4,               // matches 4 decimal places
    .qty_decimals       = 0,               // whole-unit XRP amounts
    .price_max_units    = 50.0,            // $50 max price
    .qty_max_units      = 20'000'000.0,    // 50 million XRP max order size
    .min_qty_units      = 1.0,             // minimum trade size: 1 XRP
    .min_notional_units = 1.0,             // $1 minimum value
    .status             = "online"
};


// =============================================================================
//  DOGE/USD - Reference Asset Specification
// =============================================================================
inline constexpr matching_engine::conf::Instrument DOGE_USD = {
    .base_symbol       = "DOGE",
    .quote_symbol      = "USD",
    .name              = "DOGE/USD",
    .price_tick_units  = 0.00001,     // $0.00001 tick – fine granularity for low-priced asset
    .qty_tick_units    = 1.0,         // 1 DOGE step
    .price_decimals    = 5,           // up to $0.00001 precision
    .qty_decimals      = 0,           // integer DOGE quantities
    .price_max_units   = 5.0,         // up to $5.00 per DOGE
    .qty_max_units     = 1'000'000.0, // allow up to 1M DOGE per order
    .min_qty_units     = 1.0,         // minimum 1 DOGE
    .min_notional_units= 1.0,         // $1 minimum notional
    .status            = "online"
};


inline constexpr const matching_engine::conf::Instrument& get_instrument_by_name(const std::string& name) {
    if (name == BTC_USD.name) {
        return BTC_USD;
    }
    else if (name == ETH_USD.name) {
        return ETH_USD;
    }
    else if (name == XRP_USD.name) {
        return XRP_USD;
    }
    else if (name == LTC_USD.name) {
        return LTC_USD;
    }
    else if (name == SOL_USD.name) {
        return SOL_USD;
    }
    else if (name == DOGE_USD.name) {
        return DOGE_USD;
    }
    return BTC_USD; // default
}

} // namespace flashstrike

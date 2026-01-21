#pragma once

/*
===============================================================================
Wirekrak Market — Public API Contract (v1)
===============================================================================

This header defines the complete public surface of the Wirekrak Market API.

Wirekrak Market is a semantic, correctness-oriented API for consuming market
data as meaningful, stateful market streams (e.g. trades, order books),
with explicit behavioral guarantees and policy-driven semantics.

The Market API sits above the Core protocol layer and encodes exchange-specific
market meaning. It is intentionally NOT exchange-agnostic.

This layer is designed for trading systems, analytics pipelines, and product
infrastructure that require correctness guarantees, replay semantics, and
explicit behavioral control.

Design guarantees:
  - Explicit and declarative market semantics
  - Policy-driven behavior (no implicit magic)
  - Stable public stream interfaces
  - Stable domain value type layouts
  - No protocol or transport internals exposed
  - No dependency on the Lite API
  - No breaking changes without a major version bump

Include this header to use the Wirekrak Market API.
===============================================================================
*/

/*
#include <wirekrak/market/client.hpp>
#include <wirekrak/market/error.hpp>
#include <wirekrak/market/policy.hpp>
#include <wirekrak/market/metrics.hpp>

// Domain value types (internal grouping, public re-export)
#include <wirekrak/market/domain/trade.hpp>
#include <wirekrak/market/domain/book.hpp>
*/

namespace wirekrak::market {

/*
    // ---------------------------------------------------------------------
    // Primary API Objects
    // ---------------------------------------------------------------------

    using Client = wirekrak::market::Client;

    // ---------------------------------------------------------------------
    // Market Streams (Semantic, Stateful)
    // ---------------------------------------------------------------------

    using Trades    = wirekrak::market::Trades;
    using OrderBook = wirekrak::market::OrderBook;

    // ---------------------------------------------------------------------
    // Domain Value Types (Stable)
    // ---------------------------------------------------------------------

    using Trade = wirekrak::market::domain::Trade;
    using Book  = wirekrak::market::domain::Book;

    // ---------------------------------------------------------------------
    // Error Type (Stable)
    // ---------------------------------------------------------------------

    using Error = wirekrak::market::Error;

    // ---------------------------------------------------------------------
    // Behavioral Policies (Stable Surface)
    // ---------------------------------------------------------------------
    // NOTE:
    //  - Policy enums are open-ended.
    //  - New policy values may be introduced in future versions.
    //  - Consumers must not rely on exhaustive switching.
    // ---------------------------------------------------------------------

    using Consistency = wirekrak::market::Consistency;
    using Ordering    = wirekrak::market::Ordering;
    using Replay      = wirekrak::market::Replay;
*/

} // namespace wirekrak::market

#pragma once

/*
===============================================================================
Wirekrak Lite — Public API Contract (v1)
===============================================================================

This header defines the complete public surface of the Wirekrak Lite API.

Wirekrak Lite is a domain-level, user-facing API for consuming market data
without exposing protocol, lifecycle, or exchange-specific internals.

Lite is designed for application developers and data consumers who want
safe defaults, stable types, and a minimal integration surface.

Design guarantees:
  - Stable public domain type layouts
  - Stable callback signatures
  - Exchange-neutral API surface
  - No protocol or Core internals exposed
  - No breaking changes without a major version bump

Include this header to use Wirekrak Lite.
===============================================================================
*/

#include <wirekrak/lite/client.hpp>
#include <wirekrak/lite/error.hpp>
#include <wirekrak/lite/enums.hpp>

// Domain value types (internal grouping, public re-export)
#include <wirekrak/lite/domain/trade.hpp>
#include <wirekrak/lite/domain/book_level.hpp>

namespace wirekrak::lite {

    // ---------------------------------------------------------------------
    // Primary API Objects
    // ---------------------------------------------------------------------

    using Client = wirekrak::lite::Client;

    // ---------------------------------------------------------------------
    // Domain Value Types (Stable)
    // ---------------------------------------------------------------------

    using Trade     = wirekrak::lite::domain::Trade;
    using BookLevel = wirekrak::lite::domain::BookLevel;

    // ---------------------------------------------------------------------
    // Error Type (Stable)
    // ---------------------------------------------------------------------

    using Error = wirekrak::lite::Error;

    // ---------------------------------------------------------------------
    // Semantic Enums / Tags (Stable)
    // ---------------------------------------------------------------------
    // NOTE:
    //  - Enum values are NOT guaranteed to be exhaustive.
    //  - New values may be added in future versions.
    //  - Consumers must not rely on exhaustive switching.
    // ---------------------------------------------------------------------

    using Side = wirekrak::lite::Side;
    using Tag  = wirekrak::lite::Tag;

} // namespace wirekrak::lite

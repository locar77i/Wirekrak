#pragma once

#include <string_view>

/*
===============================================================================
Wirekrak Lite — Public Enums
===============================================================================

This header defines all public, user-facing enumerations exposed by
**Wirekrak Lite**.

These enums represent **stable domain concepts**, not protocol or transport
details. They are intentionally:
  - small
  - explicit
  - independent of any specific exchange implementation

Design principles:
  - Lite enums model *what the user reasons about*, not how data is transported
  - Values are semantic (e.g. buy/sell, snapshot/update), not protocol-driven
  - All Lite enums live in this single header to keep the API discoverable,
    readable, and easy to document

Versioning contract:
  - This header is part of the **Wirekrak Lite public API**
  - New enum values may be added in future versions
  - Existing enum names and semantics are considered stable (Lite v1 invariant)

===============================================================================
*/

namespace wirekrak::lite {

// -----------------------------
// Market Side
// Represents the market intent associated with an event.
// -----------------------------
enum class Side {
    Buy,
    Sell
};


// -----------------------------
// Data Tag (snapshot vs update)
// Represents the provenance of an event within the Wirekrak pipeline (must not be used as a source of business truth)
// -----------------------------
enum class Tag {
    Snapshot,
    Update
};


// String conversion (defined in .cpp)
std::string_view to_string(Side) noexcept;
std::string_view to_string(Tag) noexcept;

} // namespace wirekrak::lite

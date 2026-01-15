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
// Market side
// -----------------------------
enum class side {
    buy,
    sell
};


// -----------------------------
// Data origin (snapshot vs update)
// -----------------------------
enum class origin {
    snapshot,
    update
};


// String conversion (defined in .cpp)
std::string_view to_string(side) noexcept;
std::string_view to_string(origin) noexcept;

} // namespace wirekrak::lite

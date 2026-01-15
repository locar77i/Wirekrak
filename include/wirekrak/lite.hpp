#pragma once

/*
===============================================================================
Wirekrak Lite — Public SDK API (v1)
===============================================================================

This header exposes the complete, stable Wirekrak Lite API.

Lite is a compiled, user-facing façade built on top of Wirekrak Core.
It provides a simplified, readable interface suitable for applications,
examples, and SDK consumers.

Design guarantees:
  - Stable DTO layouts
  - Stable callback signatures
  - No Core or protocol internals exposed
  - No breaking changes without a major version bump

Include this header to use Wirekrak Lite.
===============================================================================
*/

#include "wirekrak/lite/kraken/client.hpp"

// DTOs
#include "wirekrak/lite/dto/trade.hpp"
#include "wirekrak/lite/dto/book_level.hpp"

// Enums
#include "wirekrak/lite/enums.hpp"

#pragma once

#include <cstddef>

namespace wirekrak::core::config {

/*
===============================================================================
SPSC Ring Buffer Sizes
===============================================================================

Ring sizes are chosen based on expected message frequency and burst behavior.

Design principles:
  - Small control-plane rings (pong, status, acks)
  - Large data-plane rings (trade, book updates)
  - All sizes are compile-time constants
  - No magic numbers scattered across the codebase
===============================================================================
*/

// -----------------------------------------------------------------------------
// Control-plane messages (low frequency)
// -----------------------------------------------------------------------------
inline constexpr std::size_t rejection_ring       = 1 << 5; // 32

// -----------------------------------------------------------------------------
// Subscription acknowledgements (very low frequency)
// -----------------------------------------------------------------------------
inline constexpr std::size_t subscribe_ack_ring   = 1 << 5; // 32
inline constexpr std::size_t unsubscribe_ack_ring = 1 << 5; // 32

// -----------------------------------------------------------------------------
// High-throughput market data
// -----------------------------------------------------------------------------
inline constexpr std::size_t trade_update_ring    = 1 << 10; // 1024
inline constexpr std::size_t book_update_ring     = 1 << 10; // 1024

} // namespace wirekrak::core::config

#pragma once

#include <cstddef>

namespace wirekrak::core::protocol::config {

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
inline constexpr std::size_t REJECTION_RING_CAPACITY  = 1 << 5; // 32

// -----------------------------------------------------------------------------
// Subscription acknowledgements (very low frequency)
// -----------------------------------------------------------------------------
inline constexpr std::size_t ACK_RING_CAPACITY        = 1 << 5; // 32

// -----------------------------------------------------------------------------
// High-throughput market data
// -----------------------------------------------------------------------------
inline constexpr std::size_t TRADE_RING_CAPACITY      = 1 << 10; // 1024
inline constexpr std::size_t BOOK_RING_CAPACITY       = 1 << 10; // 1024


/*
===============================================================================
Transmission buffer capacity
===============================================================================
*/
static constexpr std::size_t TX_BUFFER_CAPACITY = 4096;

} // namespace wirekrak::core::protocol::config

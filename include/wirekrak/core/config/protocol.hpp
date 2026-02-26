#pragma once

#include <cstddef>


namespace wirekrak::core::config::protocol {

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
inline constexpr static std::size_t REJECTION_RING_CAPACITY  = 1 << 5; // 32

// -----------------------------------------------------------------------------
// Subscription acknowledgements (very low frequency)
// -----------------------------------------------------------------------------
inline constexpr static std::size_t ACK_RING_CAPACITY        = 1 << 5; // 32

// -----------------------------------------------------------------------------
// High-throughput market data
// -----------------------------------------------------------------------------
inline constexpr static std::size_t TRADE_RING_CAPACITY      = 1 << 10; // 1024
inline constexpr static std::size_t BOOK_RING_CAPACITY       = 1 << 10; // 1024


// -----------------------------------------------------------------------------
// Transmission buffer capacity
// -----------------------------------------------------------------------------
inline constexpr static std::size_t TX_BUFFER_CAPACITY = 4096;


// -----------------------------------------------------------------------------
// Message batch processing limits
// -----------------------------------------------------------------------------

inline constexpr static std::size_t MAX_MESSAGES_PER_POLL = 128; // Tunable limit for batch processing in poll()

} // namespace wirekrak::core::config::protocol

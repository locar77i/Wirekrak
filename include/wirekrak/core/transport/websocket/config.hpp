/*
================================================================================
Wirekrak WebSocket Transport Configuration
================================================================================

This header defines compile-time constants that govern the behavior and memory
layout of the WebSocket transport layer.

These values are intentionally centralized to ensure:

  • ABI stability across layers (WebSocket → Connection → Session)
  • Deterministic memory layout
  • Compile-time sizing (no dynamic allocation in hot path)
  • Explicit tuning for ULL environments

-------------------------------------------------------------------------------
Design Philosophy
-------------------------------------------------------------------------------

The WebSocket transport uses a fixed-size message block model:

  - Each incoming WebSocket message is written directly into a preallocated
    DataBlock inside a lock-free SPSC ring.
  - No heap allocations occur in the receive loop.
  - Fragmented frames are accumulated inside a single DataBlock.
  - The block is committed only once the final frame is received.

This model trades memory density for:

  ✓ Zero copy
  ✓ Zero heap contention
  ✓ Deterministic latency
  ✓ Cache predictability
  ✓ Backpressure visibility

-------------------------------------------------------------------------------
Tuning Guidance
-------------------------------------------------------------------------------

RX_BUFFER_SIZE should:

  - Cover >99% of expected WebSocket messages in a single block
  - Remain small enough to stay cache-friendly
  - Avoid pathological memory waste at high ring capacities

For Kraken v2 traffic:

  8 KB has shown to be optimal in practice:
    - Most trade / book deltas < 2 KB
    - Snapshots occasionally larger but rare
    - Fragmentation uncommon
    - Fits well within L2 cache

If targeting other exchanges or larger snapshots, increase carefully.

-------------------------------------------------------------------------------
Memory Footprint Example
-------------------------------------------------------------------------------

If:

    RX_BUFFER_SIZE = 8192 bytes
    Ring capacity   = 256

Total static memory:

    8192 * 256 = 2 MB

This is acceptable in serious ULL trading systems and eliminates runtime
allocation completely.

-------------------------------------------------------------------------------
NOTE
-------------------------------------------------------------------------------

Telemetry shows 8–16 KB is optimal: 
 - Big enough to hold the 99th percentile message comfortably, small enough to stay cache-friendly.
 - 8 KB buffers give us the best balance of cache locality and correctness, with no measurable downside for Kraken traffic.

-------------------------------------------------------------------------------
IMPORTANT
-------------------------------------------------------------------------------

Changing RX_BUFFER_SIZE changes:

  - DataBlock size
  - Ring memory footprint
  - ABI of transport layer

Do not modify casually.

================================================================================
*/
#pragma once

#include <cstddef>


namespace wirekrak::core::transport::websocket {

/// Maximum size (in bytes) of a single received WebSocket message.
/// Must accommodate full message including all fragments.
inline constexpr static std::size_t RX_BUFFER_SIZE = 8 * 1024;

} // namespace wirekrak::core::transport::websocket

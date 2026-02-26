/*
================================================================================
Wirekrak WebSocket DataBlock
================================================================================

DataBlock represents a single complete WebSocket message stored inside the
transport's SPSC ring buffer.

It is:

  • Fixed-size
  • Preallocated
  • Written by the receive thread (producer)
  • Read by the upper layer (consumer)
  • Explicitly released by the consumer

-------------------------------------------------------------------------------
Ownership Model
-------------------------------------------------------------------------------

Producer (WebSocket receive thread):
  - Acquires slot via acquire_producer_slot()
  - Writes message fragments directly into data[]
  - Sets size
  - Commits slot via commit_producer_slot()

Consumer (Connection / Session layer):
  - Calls peek_consumer_slot()
  - Reads data[0..size)
  - Calls release_consumer_slot()

IMPORTANT:

  DataBlock memory is owned by the transport ring.
  Upper layers MUST NOT:

      • Store pointers beyond release
      • Retain references after release
      • Modify memory

It is transient memory.

-------------------------------------------------------------------------------
Design Characteristics
-------------------------------------------------------------------------------

  ✓ Zero heap allocations
  ✓ Zero copy message handoff
  ✓ Deterministic lifetime
  ✓ Cacheline-aligned
  ✓ Wait-free SPSC compatible

-------------------------------------------------------------------------------
Memory Layout
-------------------------------------------------------------------------------

  [ size (4 bytes) ][ message bytes up to DATA_BLOCK_SIZE ]

Unused tail bytes remain uninitialized and must not be accessed.

-------------------------------------------------------------------------------
Alignment
-------------------------------------------------------------------------------

Aligned to 64 bytes to:

  • Reduce false sharing
  • Improve cache predictability
  • Align with SPSC ring padding strategy

-------------------------------------------------------------------------------
ABI Warning
-------------------------------------------------------------------------------

Changing DATA_BLOCK_SIZE changes the size of this struct and therefore the
memory footprint of the message ring.

================================================================================
*/

#pragma once

#include <cstdint>
#include <cstddef>
#include "wirekrak/core/config/transport/websocket.hpp"


namespace wirekrak::core::transport::websocket {

struct alignas(64) DataBlock {
    std::uint32_t size{0};                     // Number of valid bytes in data[]
    char data[RX_BUFFER_SIZE];                // Raw WebSocket message payload
};

static_assert(sizeof(DataBlock) >= RX_BUFFER_SIZE, "DataBlock size invariant violated");

} // namespace wirekrak::core::transport::websocket

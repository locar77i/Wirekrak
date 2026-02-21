#pragma once

/*
================================================================================
Wirekrak Core — Kraken Client Architecture
================================================================================

This file defines the **primary entry point** for the Wirekrak Kraken client:

    wirekrak::core::Session

It is a thin, explicit composition of:
  - a transport-level Connection
  - a protocol-level Kraken Session
  - a concrete WebSocket backend (WinHTTP)

No additional abstraction, threading, or execution model is hidden here.

-------------------------------------------------------------------------------
Execution Model (Current)
-------------------------------------------------------------------------------

Wirekrak Core operates using a **strict, deterministic 2-thread model**:

    [1] Transport / Network Thread
    [2] User / Application Thread

This separation is fundamental to correctness, latency guarantees, and safety.

-------------------------------------------------------------------------------
1. Transport Thread (WinHTTP WebSocket Thread)
-------------------------------------------------------------------------------

This thread is created and owned entirely by the WinHTTP WebSocket API.

Wirekrak does NOT control its lifetime, scheduling, or execution frequency.

Responsibilities:
  - Waits for incoming WebSocket frames
  - Receives raw bytes from the network
  - Performs minimal framing and validation
  - Executes lightweight JSON parsing
  - Routes parsed messages into lock-free SPSC rings

Hard guarantees:
  - NEVER invokes user code
  - NEVER blocks on user-controlled resources
  - NEVER allocates on hot paths
  - Performs only bounded, O(1) work per message

This thread exists solely to move data from the network
into deterministic, bounded queues.

-------------------------------------------------------------------------------
2. Application Thread (User-Owned)
-------------------------------------------------------------------------------

This is the thread that calls:

    Session::poll()

Typically the application's main loop thread.

Responsibilities:
  - Drives all forward progress explicitly
  - Drains SPSC rings populated by the transport thread
  - Dispatches typed protocol events
  - Executes all user callbacks synchronously
  - Owns all protocol-level logic and side effects

Hard guarantees:
  - User callbacks NEVER run on the transport thread
  - Network I/O can NEVER be stalled by user code
  - No locks or blocking are required for user logic
  - All execution is explicit, ordered, and observable

-------------------------------------------------------------------------------
Why Exactly Two Threads?
-------------------------------------------------------------------------------

For real-time exchange feeds (including Kraken WebSocket v2),
this model provides the best tradeoff between:

  - Latency
  - Determinism
  - CPU efficiency
  - Debuggability
  - Testability

JSON parsing cost is amortized efficiently,
ring-buffer routing is constant-time,
and callback dispatch is fully controlled by the application.

For typical workloads (thousands to tens of thousands of messages/sec),
this architecture remains comfortably within budget.

-------------------------------------------------------------------------------
Future Extension: Optional Parser Thread (Not Enabled)
-------------------------------------------------------------------------------

The architecture intentionally allows promotion to a **3-thread model**
without redesigning public APIs:

    Network Thread
        → receives raw frames
        → pushes raw JSON into a raw SPSC ring

    Parser Thread
        → pops raw JSON
        → performs full parsing / normalization
        → pushes typed events into per-channel SPSC rings

    Application Thread
        → polls
        → dispatches callbacks

This extension is appropriate only when:
  - Parsing becomes a measurable bottleneck
  - Message rates exceed ~100k/sec
  - Network jitter must be minimized further
  - Heavy schema normalization is required

The current SDK does NOT enable this mode by default.
Simplicity and determinism are preserved unless proven insufficient.

-------------------------------------------------------------------------------
What This Entry Point Represents
-------------------------------------------------------------------------------

This header defines:

    using Session = protocol::kraken::Session<winhttp::WebSocket>;

That is:
  - A concrete Kraken protocol client
  - Bound to a specific transport backend
  - With no hidden execution model
  - Fully poll-driven
  - Fully deterministic

There is no global state.
There are no background worker threads.
There is no implicit progress.

If progress occurs, it is because `poll()` was called.

-------------------------------------------------------------------------------
Summary
-------------------------------------------------------------------------------

Current model:

    [Transport Thread]
        recv → parse → push_to_ring

    [Application Thread]
        poll → pop_from_ring → callbacks

Future optional model:

    recv → push_raw
        → parse_thread → push_typed
            → poll → callbacks

Wirekrak Core is built to evolve without breaking contracts.

================================================================================
*/

#include "wirekrak/core/transport/concepts.hpp"
#include "wirekrak/core/transport/websocket/data_block.hpp"
#include "wirekrak/core/transport/winhttp/websocket.hpp"
#include "wirekrak/core/transport/connection.hpp"
#include "wirekrak/core/protocol/kraken/session.hpp"


namespace wirekrak::core {

    using MessageRingT = lcr::lockfree::spsc_ring<transport::websocket::DataBlock, transport::RX_RING_CAPACITY>;

namespace transport {

    using WebSocketT   = winhttp::WebSocketImpl<MessageRingT>;

    // Assert that WebSocketT conforms to transport::WebSocketConcept concept
    static_assert(WebSocketConcept<WebSocketT>);
  
    using ConnectionT  = Connection<WebSocketT, MessageRingT>;

} // namespace transport

namespace protocol::kraken {
    
    using SessionT     = protocol::kraken::Session<transport::WebSocketT, MessageRingT>;

} // namespace protocol::kraken

} // namespace wirekrak::core

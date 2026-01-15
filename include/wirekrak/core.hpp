#pragma once

/*
================================================================================
Wirekrak Architecture Overview
================================================================================

This SDK currently implements a **2-thread processing model** designed to be:
- minimal
- deterministic
- extremely fast
- free of locks and dynamic contention

The two active threads are:

-------------------------------------------------------------------------------
1. Network Thread (WinHTTP Receive Thread)
-------------------------------------------------------------------------------
Created internally by the WinHTTP WebSocket API and used exclusively for
asynchronous I/O.

Responsibilities:
    - Waits for incoming WebSocket frames
    - Reads raw message bytes into a temporary buffer
    - Invokes the SDK's receive callback (receive_loop)
    - Performs lightweight JSON parsing
    - Routes each parsed message into the appropriate SPSC ring buffer

This thread NEVER interacts with user code directly.
It never blocks, does not allocate, and only performs O(1) ring-buffer pushes.

-------------------------------------------------------------------------------
2. Application Thread (Main Thread)
-------------------------------------------------------------------------------
Owned by the user application, typically the thread running main().

Responsibilities:
    - Periodically calls Session::poll()
    - Pops parsed messages from the SPSC rings
    - Dispatches them to user-registered callbacks
    - Executes all user logic safely and synchronously

This ensures:
    - User code NEVER runs inside the network thread
    - No user callback can stall networking
    - No locks or atomics are required for user-level operations

-------------------------------------------------------------------------------
Why Only Two Threads?
-------------------------------------------------------------------------------
For the vast majority of real-time exchange-feed workloads, this architecture is
ideal. Parsing is efficient, routing is O(1), and the network thread remains
highly deterministic. The system performs extremely well even under sustained
high message rates (multiple thousands per second).

-------------------------------------------------------------------------------
Future Extension: Optional Dedicated Parser Thread
-------------------------------------------------------------------------------
The architecture has been intentionally designed so that adding a **third thread**
(a dedicated parser thread) is straightforward.

The upgraded model would include:

    Network Thread
        → pushes raw JSON strings into a raw_spsc_ring

    Parser Thread
        → pops raw JSON
        → performs parsing and normalization
        → pushes typed events into per-channel SPSC rings

    Main Thread
        → pops typed events
        → dispatches callbacks

This 3-thread model is beneficial if:
    - Message throughput exceeds ~100,000 messages/sec
    - Parsing becomes a measurable bottleneck
    - Ultra-low network jitter is required
    - Multiple message types require heavy parsing

The current 2-thread design remains simpler, cooler on CPU load, and entirely
sufficient for typical Kraken/WebSocket API usage.

-------------------------------------------------------------------------------
Summary
-------------------------------------------------------------------------------
[Thread 1] WinHTTP Network Thread:
    recv → parse → push_to_ring

[Thread 2] User Application Thread:
    poll → pop_from_ring → callbacks

[Future Optional] Parser Thread:
    recv → push_raw → parse_thread → push_typed → callbacks

The SDK is built to support this evolution with minimal refactoring.

================================================================================
*/

#include "wirekrak/core/protocol/kraken/session.hpp"
#include "wirekrak/core/transport/winhttp/websocket.hpp"


namespace wirekrak::core {

using Session = protocol::kraken::Session<transport::winhttp::WebSocket>;

} // namespace wirekrak::core

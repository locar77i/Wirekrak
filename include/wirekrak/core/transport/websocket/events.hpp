#pragma once

/*
===============================================================================
 wirekrak::core::transport::websocket::Event
===============================================================================

Control-plane event type emitted by a WebSocket transport implementation
and delivered to the owning Connection via a lock-free SPSC ring buffer.

This replaces cross-thread callbacks (on_error / on_close) with a
deterministic, poll-driven, lock-free event channel.

-------------------------------------------------------------------------------
 Design Goals
-------------------------------------------------------------------------------

• No cross-thread callbacks
• No dynamic memory allocations
• Trivially copyable
• Lock-free SPSC friendly
• Deterministic delivery
• Exactly-once semantics for Close
• Lossless delivery required

-------------------------------------------------------------------------------
 Threading Model
-------------------------------------------------------------------------------

WebSocket:
    - Runs an internal IO thread
    - Pushes Event objects into an SPSC ring

Connection:
    - Runs on a single-threaded poll loop
    - Drains events via poll_event()
    - Drives state machine transitions

No state mutation is allowed across threads.

-------------------------------------------------------------------------------
 Control-Plane vs Data-Plane
-------------------------------------------------------------------------------

This event type is strictly for CONTROL-PLANE signaling:

    • Close  → Transport closed (local or remote)
    • Error  → Transport-level failure

High-frequency message delivery (data-plane) must use a separate
mechanism optimized for ULL (e.g., preallocated message buffers).

-------------------------------------------------------------------------------
 Reliability Contract
-------------------------------------------------------------------------------

• Control-plane events MUST NOT be dropped.
• If the SPSC ring is full, this is a fatal condition.
• Losing Close/Error breaks transport correctness.

-------------------------------------------------------------------------------
 Memory Model
-------------------------------------------------------------------------------

Event is:
    - Trivially copyable
    - Small POD type
    - Safe for lock-free SPSC transfer

===============================================================================
*/

#include <cstdint>
#include "wirekrak/core/transport/error.hpp"

namespace wirekrak::core::transport::websocket {

// -----------------------------------------------------------------------------
// EventType
// -----------------------------------------------------------------------------

enum class EventType : std::uint8_t {
    Close = 0,
    Error = 1,
    BackpressureDetected = 2,
    BackpressureCleared  = 3,
};

// -----------------------------------------------------------------------------
// Event
// -----------------------------------------------------------------------------

struct Event {

    EventType type;

    union {
        transport::Error error; // valid only if type == EventType::Error
    };

    // -------------------------------------------------------------------------
    // Factory: Close
    // -------------------------------------------------------------------------

    static constexpr Event make_close() noexcept {
        Event ev;
        ev.type = EventType::Close;
        return ev;
    }

    // -------------------------------------------------------------------------
    // Factory: Error
    // -------------------------------------------------------------------------

    static constexpr Event make_error(transport::Error e) noexcept {
        Event ev;
        ev.type  = EventType::Error;
        ev.error = e;
        return ev;
    }

    // -------------------------------------------------------------------------
    // Factory: Backpressure
    // -------------------------------------------------------------------------

    static constexpr Event make_backpressure_detected() noexcept {
        Event ev;
        ev.type = EventType::BackpressureDetected;
        return ev;
    }

    static constexpr Event make_backpressure_cleared() noexcept {
        Event ev;
        ev.type = EventType::BackpressureCleared;
        return ev;
    }
};

// Ensure SPSC-safety properties
static_assert(std::is_trivially_copyable_v<Event>, "websocket::Event must be trivially copyable");
static_assert(sizeof(Event) <= 16, "websocket::Event should remain small and cache-friendly");

} // namespace wirekrak::core::transport::websocket

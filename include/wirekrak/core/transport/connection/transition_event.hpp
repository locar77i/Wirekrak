#pragma once

#include <string_view>


namespace wirekrak::core::transport {

/*
===============================================================================
 Connection Transition Events (ULL-safe)
===============================================================================

TransitionEvent represents **externally observable, edge-triggered connection
events** emitted by transport::Connection via its poll_event() interface.

These events are:
  - Edge-triggered (not level-based)
  - Single-shot per transition
  - Deterministic and poll-driven
  - Allocation-free and callback-free
  - Suitable for ultra-low-latency (ULL) environments

The Connection does NOT expose its internal FSM, liveness timers, or transport
details. Only transitions that are meaningful to the user are surfaced.

-------------------------------------------------------------------------------
 Delivery Semantics
-------------------------------------------------------------------------------

- Events are pushed into a bounded, lock-free SPSC ring buffer.
- If the buffer overflows, the **oldest event is dropped**.
- Events are best-effort and may be lost if not polled in time.
- Events are NOT replayed across connection lifetimes.
- Observing an event is optional; missing an event has no side effects.

This design guarantees:
  - No hidden coupling
  - No reentrancy
  - No implicit ownership or retries
  - No timing assumptions beyond poll cadence

-------------------------------------------------------------------------------
 Event Meanings
-------------------------------------------------------------------------------

None
  No externally visible transition occurred.

Connected
  The logical connection has become fully established and usable.
  Emitted once per successful connect or reconnect.

RetryScheduled
  The connection has entered an automatic retry cycle due to a recoverable
  failure. A reconnection attempt will be scheduled according to backoff policy.

Disconnected
  The logical connection has reached a final disconnected state.
  No further automatic retries will occur unless open() is called explicitly.

LivenessThreatened
  Early warning that connection liveness is at risk.
  Emitted when protocol activity and heartbeat approach timeout thresholds.
  This is a one-shot warning per connection cycle; recovery is implicit.

===============================================================================
*/


enum class TransitionEvent : uint8_t {
    None,               // No externally visible transition
    Connected,          // Logical connection established
    Disconnected,       // Logical connection fully down
    RetryScheduled,     // Entered automatic retry cycle
    // --- Liveness ---
    LivenessThreatened, // Liveness threatened (healthy → warning)
};

[[nodiscard]]
inline std::string_view to_string(TransitionEvent ev) noexcept {
    switch (ev) {
        case TransitionEvent::None:               return "None";
        case TransitionEvent::Connected:          return "Connected";
        case TransitionEvent::Disconnected:       return "Disconnected";
        case TransitionEvent::RetryScheduled:     return "RetryScheduled";
        case TransitionEvent::LivenessThreatened: return "LivenessThreatened";
        default:                                  return "Unknown";
    }
}

} // namespace wirekrak::core::transport

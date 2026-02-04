#pragma once

#include <string_view>


namespace wirekrak::core::transport {

/*
===============================================================================
 Connection Transition Events (ULL-safe)
===============================================================================

TransitionEvent represents **externally observable connection state transitions**
emitted by transport::Connection via its poll() interface.

These events are:
  - Edge-triggered (not level-triggered)
  - Single-shot (emitted once per transition)
  - Deterministic and poll-driven
  - Allocation-free and callback-free
  - Suitable for ultra-low-latency (ULL) environments

The Connection does NOT expose its internal FSM, liveness signals, or transport
details. Only transitions that are meaningful to the user are surfaced.

-------------------------------------------------------------------------------
 Semantics
-------------------------------------------------------------------------------

- poll() returns exactly ONE TransitionEvent per call.
- Most poll() calls return TransitionEvent::None.
- A non-None event indicates that a *logical connection boundary* was crossed.
- Events are never buffered, queued, or replayed.
- If an event is not observed, it is intentionally lost.

This design guarantees:
  - No hidden state
  - No reentrancy
  - No implicit ownership or retries
  - No timing assumptions beyond poll cadence

-------------------------------------------------------------------------------
 Event Meanings
-------------------------------------------------------------------------------

None
  No externally visible transition occurred during this poll() call.
  Internal progress (timers, liveness checks, transport I/O) may still have
  occurred, but nothing crossed a logical connection boundary.

Connected
  The logical connection has become fully established and usable.
  All connection invariants are satisfied.
  This event is emitted once per successful connect or reconnect.

RetryScheduled
  The connection has entered an automatic retry cycle due to a recoverable
  failure (e.g. timeout, transport error).
  The next reconnection attempt will be scheduled according to backoff policy.

Disconnected
  The logical connection has reached a final disconnected state.
  No further automatic retries will occur unless open() is called explicitly.

===============================================================================
*/


enum class TransitionEvent : uint8_t {
    None,             // No externally visible transition
    Connected,        // Logical connection established
    Disconnected,     // Logical connection fully down
    RetryScheduled    // Entered automatic retry cycle
};

[[nodiscard]]
inline std::string_view to_string(TransitionEvent ev) noexcept {
    switch (ev) {
        case TransitionEvent::None:           return "None";
        case TransitionEvent::Connected:      return "Connected";
        case TransitionEvent::Disconnected:   return "Disconnected";
        case TransitionEvent::RetryScheduled: return "RetryScheduled";
        default:                              return "Unknown";
    }
}

} // namespace wirekrak::core::transport

/*
===============================================================================
 Connection Signals (ULL-safe)
===============================================================================

connection::Signal represents **externally observable, edge-triggered facts**
emitted by transport::Connection via its poll_signal() interface.

Signals are:
  - Edge-triggered (not level- or state-based)
  - Single-shot per occurrence
  - Deterministic and poll-driven
  - Allocation-free and callback-free
  - Suitable for ultra-low-latency (ULL) environments

The Connection does NOT expose its internal FSM, liveness timers, retry logic,
or transport internals. Only externally meaningful facts are surfaced.

Signals are **informational**, not authoritative:
they do not represent full state and are never required for correctness.

-------------------------------------------------------------------------------
 Delivery Semantics
-------------------------------------------------------------------------------

- Signals are pushed into a bounded, lock-free SPSC ring buffer
- If the buffer overflows, the **oldest signal is dropped**
- Signals are best-effort and may be lost if not polled in time
- Signals are NOT replayed across transport lifetimes
- Observing a signal is optional; missing one has no semantic impact

Progress and correctness must be inferred using:
  - transport epoch
  - rx / tx message counters

-------------------------------------------------------------------------------
 Signal Meanings
-------------------------------------------------------------------------------

None
  No externally observable signal occurred.

Connected
  A WebSocket connection has been successfully established.
  Emitted once per completed transport lifetime.
  Increments the transport epoch.

RetryScheduled
  The connection has entered an automatic retry cycle due to a recoverable
  failure. A reconnection attempt will occur according to backoff policy.

Disconnected
  The logical transport connection became unusable.
  Further automatic retries depend on configuration and failure cause.

LivenessThreatened
  Early warning that observable transport activity is approaching timeout.
  Emitted at most once per silence window.
  No state change is implied.

===============================================================================
*/


#pragma once

#include <string_view>


namespace wirekrak::core::transport::connection {


enum class Signal : uint8_t {
    None,                  // No externally observable signal
    Connected,             // Logical connection established
    Disconnected,          // Logical connection fully down
    RetryImmediate,        // Retry will occur immediately
    RetryScheduled,        // Entered automatic retry cycle
    // --- Liveness ---
    LivenessThreatened,    // Liveness threatened (healthy → warning)
    // --- Backpressure ---
    BackpressureDetected,  // Transport backpressure detected (user is not draining fast enough)
};

[[nodiscard]]
inline std::string_view to_string(Signal sig) noexcept {
    switch (sig) {
        case Signal::None:                 return "None";
        case Signal::Connected:            return "Connected";
        case Signal::Disconnected:         return "Disconnected";
        case Signal::RetryImmediate:       return "RetryImmediate";
        case Signal::RetryScheduled:       return "RetryScheduled";
        case Signal::LivenessThreatened:   return "LivenessThreatened";
        case Signal::BackpressureDetected: return "BackpressureDetected";
        default:                           return "Unknown";
    }
}

} // namespace wirekrak::core::transport::connection

/*
================================================================================
Backpressure Configuration
================================================================================

Purpose
-------
Defines compile-time thresholds controlling transport backpressure dynamics.

Backpressure handling in Wirekrak is split across layers:

  Transport:
    - Detects ring saturation.
    - Shapes overload signals using hysteresis.
    - Emits BackpressureDetected / BackpressureCleared events.

  Session:
    - Observes overload persistence.
    - Escalates (e.g. closes connection) if overload is sustained.

This configuration defines the control system parameters governing that
interaction.

-------------------------------------------------------------------------------
Hysteresis Thresholds
-------------------------------------------------------------------------------

Activation Threshold:
    Number of consecutive failed slot acquisitions required to enter
    the OVERLOADED state.

Deactivation Threshold:
    Number of consecutive successful slot acquisitions required to
    return to NORMAL state.

Strict Mode:
    - Immediate activation (1 poll).
    - Requires stable recovery before clearing.
    - Suitable for correctness-first environments.

Relaxed Mode:
    - Tolerates short bursts before activation.
    - Same recovery stabilization window as strict.
    - Suitable for burst-heavy but recoverable traffic.

Hysteresis suppresses oscillation and prevents control-event flooding.

-------------------------------------------------------------------------------
Escalation Thresholds
-------------------------------------------------------------------------------

Escalation is handled by the Session layer and represents sustained overload.

Invariant:
    ESCALATION_THRESHOLD > DEACTIVATION_THRESHOLD

This guarantees the system has at least one full recovery window before
escalation becomes possible.

Escalation thresholds are intentionally derived from hysteresis values to:

  - Prevent premature shutdown.
  - Ensure recovery is theoretically possible.
  - Maintain deterministic control behavior.

-------------------------------------------------------------------------------
Design Properties
-------------------------------------------------------------------------------

- Fully constexpr (no runtime configuration).
- Deterministic and branch-minimal.
- No dynamic memory.
- Suitable for ultra-low-latency environments.
- Explicit separation between signal shaping and policy escalation.

These values should be tuned relative to:
  - Ring buffer capacity
  - Expected message rate
  - Consumer drain speed
  - Acceptable recovery latency

================================================================================
*/
#pragma once

#include <cstddef>


namespace wirekrak::core::config::backpressure {

// -----------------------------------------------------------------------------
// Backpressure hysteresis thresholds
// -----------------------------------------------------------------------------

inline constexpr static std::uint32_t STRICT_LOCAL_SPINS                        =   32;
inline constexpr static std::uint32_t STRICT_HYSTERESIS_ACTIVATION_THRESHOLD    =    1;
inline constexpr static std::uint32_t STRICT_HYSTERESIS_DEACTIVATION_THRESHOLD  = 1024;

inline constexpr static std::uint32_t RELAXED_LOCAL_SPINS =                       32;
inline constexpr static std::uint32_t RELAXED_HYSTERESIS_ACTIVATION_THRESHOLD   = 1024;
inline constexpr static std::uint32_t RELAXED_HYSTERESIS_DEACTIVATION_THRESHOLD = 1024;

// -----------------------------------------------------------------------------
// Backpressure escalation thresholds
// -----------------------------------------------------------------------------

inline constexpr static std::uint32_t STRICT_ESCALATION_THRESHOLD  = (1 << 12); // overload active for 4096 consecutive polls
inline constexpr static std::uint32_t RELAXED_ESCALATION_THRESHOLD = (1 << 24); // overload active for 16777216 consecutive polls

} // namespace wirekrak::core::config::backpressure

#pragma once

#include <cstdint>
#include <atomic>

#include "lcr/system/monotonic_clock.hpp"

namespace wirekrak::core {
namespace protocol {
namespace fact {

/*
================================================================================
 Pong Fact (Core)
================================================================================

This header defines the **Pong fact** exposed by Wirekrak Core.

The Kraken WebSocket protocol defines a rich `schema::system::Pong` message that
includes request correlation, warnings, exchange timestamps, and error details.
While valuable for diagnostics and application-level logic, that schema is
*intentionally not exposed* by Core.

Core design principle:
  Core must never execute user code, queue non-essential events, or expose
  protocol messages whose semantics encourage event-driven or re-entrant usage.

Instead, Core exposes only **irreducible protocol facts** — minimal, stable,
pull-based observations that are sufficient to reason about correctness and
liveness without introducing control-flow inversion or reentrancy hazards.

--------------------------------------------------------------------------------
 Why not expose `schema::system::Pong`?
--------------------------------------------------------------------------------

The full Pong schema includes:
  - Optional request identifiers (`req_id`)
  - Success / error signaling
  - Diagnostic warnings
  - Exchange-provided timestamps

These fields are intentionally discarded at the Core boundary because:

  • Liveness is independent of request success or correlation.
  • Diagnostic and error semantics belong to higher layers (Lite / tooling).
  • Exchange timestamps are not monotonic and are not comparable to local clocks.
  • Exposing message-shaped data encourages edge-driven APIs and callbacks.

Including these fields would weaken Core’s determinism guarantees and invite
re-entrant designs that are incompatible with ultra-low-latency systems.

--------------------------------------------------------------------------------
 What Core exposes instead
--------------------------------------------------------------------------------

Core reduces Pong to two **level-triggered facts**:

  1. A monotonic receipt counter
     - Answers: “Has a pong been observed since X?”
     - Replaces event-style pong notifications.

  2. A monotonic local receive timestamp (nanoseconds)
     - Answers: “How long since the last proof of liveness?”
     - Derived from a TSC-based monotonic clock.
     - Guaranteed to be strictly increasing on the parsing thread.

These two fields are sufficient to derive all Core-level liveness predicates
without allocation, callbacks, rings, or user code execution.

--------------------------------------------------------------------------------
 Threading & Ordering Guarantees
--------------------------------------------------------------------------------

  • Updates occur on the Session’s parsing thread.
  • Reads may occur from any thread.
  • Atomic operations use relaxed ordering:
      - These fields convey observational facts only.
      - No cross-field or cross-component synchronization is implied.

The timestamp is monotonic per-thread, not globally ordered. This is by design
and avoids global contention while remaining correct for liveness evaluation.

================================================================================
*/

struct PongFact {
    // Monotonic count of pong receipts observed by the Session.
    // Used as an epoch-style indicator for edge detection.
    std::atomic<std::uint64_t> count{0};

    // Monotonic local receive timestamp (nanoseconds).
    // Derived from a TSC-based monotonic clock.
    std::atomic<std::uint64_t> last_rx_ts_ns{0};

    // Record the observation of a pong.
    // Intended to be called exclusively by Core on the parsing thread.
    inline void record() noexcept {
        const std::uint64_t now =
            lcr::system::monotonic_clock::instance().now_ns();

        last_rx_ts_ns.store(now, std::memory_order_relaxed);
        count.fetch_add(1, std::memory_order_relaxed);
    }

    // Read-only helpers (optional, convenience)
    [[nodiscard]]
    inline std::uint64_t count() const noexcept {
        return count.load(std::memory_order_relaxed);
    }

    [[nodiscard]]
    inline std::uint64_t last_ts_ns() const noexcept {
        return last_rx_ts_ns.load(std::memory_order_relaxed);
    }
};

} // namespace fact
} // namespace protocol
} // namespace wirekrak::core

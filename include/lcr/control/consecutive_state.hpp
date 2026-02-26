#pragma once

#include <cstdint>

namespace lcr::control {

/*
================================================================================
Consecutive State Counters
================================================================================

Purpose
-------
These utilities track consecutive frames (or polls) during which a condition
remains active.

They are generic temporal control primitives and intentionally contain:

  • No logging
  • No allocation
  • No policy logic
  • No domain-specific semantics

They are designed for single-threaded control loops (e.g. receive loops,
poll loops, event loops) where state stability over time must be measured.

Typical use cases:
  - Backpressure escalation
  - Fault persistence detection
  - Liveness degradation tracking
  - Stability enforcement
  - Circuit-breaker timing logic

================================================================================
Design Principles
================================================================================

- Deterministic
- Branch-light
- Zero dynamic memory
- Zero external dependencies
- O(1) cost per frame
- ULL-safe
- Explicit frame advancement (via next_frame())

================================================================================
*/


// =============================================================================
// ConsecutiveStateCounter
// =============================================================================
//
// Tracks consecutive frames during which a state remains active.
//
// Behavior:
//   - set_active(true)  -> marks the condition active for this frame
//   - set_active(false) -> marks inactive
//   - next_frame()      -> advances the temporal window
//
// If active during next_frame():
//   consecutive count increments
//
// If inactive during next_frame():
//   consecutive count resets to zero
//
// This variant remembers whether the state was active across frames.
// =============================================================================

class ConsecutiveStateCounter {
    bool active_{false};
    std::uint32_t consecutive_{0};

public:

    // Marks the state for the current frame.
    inline void set_active(bool value) noexcept {
        active_ = value;
    }

    // Advances the temporal window (call once per frame/poll).
    inline void next_frame() noexcept {
        if (active_) {
            ++consecutive_;
        } else {
            consecutive_ = 0;
        }
    }

    // Returns whether the state is currently marked active.
    [[nodiscard]]
    inline bool is_active() const noexcept {
        return active_;
    }

    // Returns number of consecutive active frames.
    [[nodiscard]]
    inline std::uint32_t count() const noexcept {
        return consecutive_;
    }

    // Resets internal state.
    inline void reset() noexcept {
        active_ = false;
        consecutive_ = 0;
    }
};


// =============================================================================
// FrameConsecutiveStateCounter
// =============================================================================
//
// Aggregates activity inside a frame and converts it into consecutive-frame
// tracking.
//
// Behavior:
//   - mark_active()  -> indicates that activity occurred during this frame
//   - next_frame()   -> advances frame boundary
//
// If mark_active() was called before next_frame():
//   consecutive count increments
//
// Otherwise:
//   consecutive count resets to zero
//
// The internal "active_this_frame" flag is automatically cleared at each
// next_frame() call.
//
// Useful when multiple signals may occur within a frame and should collapse
// into a single "frame was active" decision.
// =============================================================================

class FrameConsecutiveStateCounter {
    bool active_this_frame_{false};
    std::uint32_t consecutive_{0};

public:

    // Marks that activity occurred during the current frame.
    inline void mark_active() noexcept {
        active_this_frame_ = true;
    }

    // Advances the temporal window (call once per frame/poll).
    inline void next_frame() noexcept {
        if (active_this_frame_) {
            ++consecutive_;
        } else {
            consecutive_ = 0;
        }

        active_this_frame_ = false;
    }

    // Returns true if the state has been active for one or more consecutive frames.
    [[nodiscard]]
    inline bool is_active() const noexcept {
        return consecutive_ > 0;
    }

    // Returns number of consecutive active frames.
    [[nodiscard]]
    inline std::uint32_t count() const noexcept {
        return consecutive_;
    }

    // Resets internal state.
    inline void reset() noexcept {
        active_this_frame_ = false;
        consecutive_ = 0;
    }
};

} // namespace lcr::control

#pragma once

#include <cstdint>

namespace lcr::control {

/*
================================================================================
BinaryHysteresis
================================================================================

Purpose
-------
A deterministic two-state machine with independent activation and
deactivation thresholds.

It collapses oscillating boolean signals into stable transitions.

State Diagram
-------------

    Inactive
        │
        │  (activate_threshold consecutive active signals)
        ▼
    Active
        │
        │  (deactivate_threshold consecutive inactive signals)
        ▼
    Inactive


Design Properties
-----------------
- Activation and deactivation thresholds are compile-time constants.
- No allocations.
- No atomics.
- No locks.
- No exceptions.
- Fully inlinable.
- Single-threaded usage (e.g., transport receive loop).

Typical Usage
-------------

Strict policy:
    using StrictHysteresis = BinaryHysteresis<1, 8>;

Relaxed policy:
    using RelaxedHysteresis = BinaryHysteresis<50, 8>;

Then:

    auto t = hysteresis.on_active_signal();
    auto t = hysteresis.on_inactive_signal();

Transitions are emitted exactly once per stable change.

================================================================================
*/

template<
    std::uint32_t ActivateThreshold,
    std::uint32_t DeactivateThreshold
>
class BinaryHysteresis {
    static_assert(ActivateThreshold > 0, "ActivateThreshold must be > 0");
    static_assert(DeactivateThreshold > 0, "DeactivateThreshold must be > 0");

public:

    enum class State : std::uint8_t {
        Inactive,
        Active
    };

    enum class Transition : std::uint8_t {
        None,
        Activated,
        Deactivated
    };

public:

    constexpr BinaryHysteresis() noexcept = default;

    // Called when the "active" condition is observed
    // (e.g., ring full)
    [[nodiscard]]
    inline Transition on_active_signal() noexcept {
        deactivate_streak_ = 0;

        if (state_ == State::Inactive) {
            if (++activate_streak_ >= ActivateThreshold) {
                state_ = State::Active;
                activate_streak_ = 0;
                return Transition::Activated;
            }
        } else {
            // Already active — collapse oscillation
            activate_streak_ = 0;
        }

        return Transition::None;
    }

    // Called when the "inactive" condition is observed
    // (e.g., slot successfully acquired)
    [[nodiscard]]
    inline Transition on_inactive_signal() noexcept {
        activate_streak_ = 0;

        if (state_ == State::Active) {
            if (++deactivate_streak_ >= DeactivateThreshold) {
                state_ = State::Inactive;
                deactivate_streak_ = 0;
                return Transition::Deactivated;
            }
        } else {
            // Already inactive — collapse oscillation
            deactivate_streak_ = 0;
        }

        return Transition::None;
    }

    [[nodiscard]]
    constexpr State state() const noexcept {
        return state_;
    }

    inline void reset() noexcept {
        state_ = State::Inactive;
        activate_streak_ = 0;
        deactivate_streak_ = 0;
    }

private:
    State state_{State::Inactive};

    std::uint32_t activate_streak_{0};
    std::uint32_t deactivate_streak_{0};
};

} // namespace lcr::control
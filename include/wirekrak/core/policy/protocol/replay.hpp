#pragma once

#include <concepts>
#include <ostream>

namespace wirekrak::core::policy::protocol {

// ----------------------------------------------------------------------------
// Concept
// ----------------------------------------------------------------------------

template<class T>
concept HasReplayMember =
    requires {
        { T::enabled } -> std::same_as<const bool&>;
    };

template<class T>
concept ReplayConcept = HasReplayMember<T>;

namespace replay {

// ============================================================================
// Replay Policy
// ============================================================================
//
// Controls whether the Session replays previously acknowledged subscriptions
// after a reconnect.
//
// Design goals:
//   • Compile-time injectable
//   • Zero runtime branching
//   • No state
//   • Deterministic behavior
//   • Ultra-low-latency friendly
//
// Semantics:
//
// Disabled
//   - No subscription replay occurs
//   - Session starts empty after reconnect
//
// Enabled
//   - Session replays previously acknowledged subscriptions
//   - Replay database drives deterministic re-subscription
//
// NOTE:
// The policy only exposes configuration. The replay algorithm remains fully
// implemented inside Session.
//
// ============================================================================


// ----------------------------------------------------------------------------
// Disabled Replay Policy
// ----------------------------------------------------------------------------

struct Disabled {

    // Whether replay is enabled
    static constexpr bool enabled = false;

    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // ------------------------------------------------------------

    static constexpr const char* mode_name() noexcept {
        return "Disabled";
    }

    static void dump(std::ostream& os) {
        os << "[Protocol Replay Policy]\n";
        os << "- Mode        : " << mode_name() << "\n";
        os << "- Enabled     : no\n\n";
    }
};

// Assert that Disabled satisfies the ReplayConcept
static_assert(ReplayConcept<Disabled>, "Disabled does not satisfy ReplayConcept");


// ----------------------------------------------------------------------------
// Enabled Replay Policy
// ----------------------------------------------------------------------------

struct Enabled {

    // Whether replay is enabled
    static constexpr bool enabled = true;

    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // ------------------------------------------------------------

    static constexpr const char* mode_name() noexcept {
        return "Enabled";
    }

    static void dump(std::ostream& os) {
        os << "[Protocol Replay Policy]\n";
        os << "- Mode        : " << mode_name() << "\n";
        os << "- Enabled     : yes\n\n";
    }
};

// Assert that Enabled satisfies the ReplayConcept
static_assert(ReplayConcept<Enabled>, "Enabled does not satisfy ReplayConcept");

} // namespace replay


// ----------------------------------------------------------------------------
// Default
// ----------------------------------------------------------------------------

using DefaultReplay = replay::Enabled;

// Assert that DefaultReplay satisfies the ReplayConcept
static_assert(ReplayConcept<DefaultReplay>, "DefaultReplay does not satisfy ReplayConcept");

} // namespace wirekrak::core::policy::protocol

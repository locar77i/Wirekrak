
#pragma once

#include <concepts>

namespace wirekrak::core::policy::protocol {
namespace liveness {

// ============================================================================
// Liveness Policy
// ============================================================================
//
// Controls how the Session reacts to transport::connection::Signal::LivenessThreatened.
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
// Passive
//   - Session reflects observable protocol traffic only
//   - No proactive ping is sent
//
// Active
//   - Session proactively maintains liveness
//   - Sends ping() when liveness is threatened
//
// ============================================================================


// ----------------------------------------------------------------------------
// Passive Liveness Policy
// ----------------------------------------------------------------------------

struct Passive {

    // Whether session should actively maintain liveness
    static constexpr bool proactive = false;
};


// ----------------------------------------------------------------------------
// Active Liveness Policy
// ----------------------------------------------------------------------------

struct Active {

    // Whether session should actively maintain liveness
    static constexpr bool proactive = true;
};

} // namespace liveness

// ----------------------------------------------------------------------------
// Concept
// ----------------------------------------------------------------------------

template<class T>
concept HasLivenessMember =
    requires {
        { T::proactive } -> std::same_as<const bool&>;
    };

template<class T>
concept LivenessConcept =
    HasLivenessMember<T>;


// ----------------------------------------------------------------------------
// Default
// ----------------------------------------------------------------------------

using DefaultLiveness = liveness::Passive;

} // namespace wirekrak::core::policy::protocol

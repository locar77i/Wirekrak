
#pragma once

#include <concepts>
#include <ostream>

namespace wirekrak::core::policy::protocol {

// ----------------------------------------------------------------------------
// Concept
// ----------------------------------------------------------------------------

template<class T>
concept HasLivenessMember =
    requires {
        { T::proactive } -> std::same_as<const bool&>;
    };

template<class T>
concept LivenessConcept = HasLivenessMember<T>;


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

    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // ------------------------------------------------------------

    static constexpr const char* mode_name() noexcept {
        return "Passive";
    }

    static void dump(std::ostream& os) {
        os << "[Protocol Liveness Policy]\n";
        os << "- Mode        : " << mode_name() << "\n";
        os << "- Proactive   : no\n\n";
    }
};

// Assert that Passive satisfies the LivenessConcept
static_assert(LivenessConcept<Passive>, "Passive does not satisfy LivenessConcept");


// ----------------------------------------------------------------------------
// Active Liveness Policy
// ----------------------------------------------------------------------------

struct Active {

    // Whether session should actively maintain liveness
    static constexpr bool proactive = true;

    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // ------------------------------------------------------------

    static constexpr const char* mode_name() noexcept {
        return "Active";
    }

    static void dump(std::ostream& os) {
        os << "[Protocol Liveness Policy]\n";
        os << "- Mode        : " << mode_name() << "\n";
        os << "- Proactive   : yes\n\n";
    }
};

// Assert that Active satisfies the LivenessConcept
static_assert(LivenessConcept<Active>, "Active does not satisfy LivenessConcept");

} // namespace liveness


// ----------------------------------------------------------------------------
// Default
// ----------------------------------------------------------------------------

using DefaultLiveness = liveness::Passive;

// Assert that DefaultLiveness satisfies the LivenessConcept
static_assert(LivenessConcept<DefaultLiveness>, "DefaultLiveness does not satisfy LivenessConcept");

} // namespace wirekrak::core::policy::protocol

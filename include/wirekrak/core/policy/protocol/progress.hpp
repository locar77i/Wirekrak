#pragma once

#include <concepts>
#include <cstdint>
#include <ostream>

#include "lcr/format.hpp"


namespace wirekrak::core::policy::protocol {

// ----------------------------------------------------------------------------
// Concept
// ----------------------------------------------------------------------------

template<class T>
concept HasProgressMembers =
    requires {
        { T::enabled } -> std::convertible_to<bool>;
        { T::timeout_ns } -> std::convertible_to<std::uint64_t>;
    };

template<class T>
concept ProgressConcept = HasProgressMembers<T>;


// ----------------------------------------------------------------------------
// Namespace
// ----------------------------------------------------------------------------

namespace progress {

// ============================================================================
// Progress (Convergence / Stall) Policy
// ============================================================================
//
// Defines how the system interprets lack of observable protocol progress.
//
// This policy does NOT track progress itself. Instead, it configures how long
// the system tolerates the absence of progress before declaring the protocol
// "idle".
//
// This is used by higher layers:
//
//     Manager    → symbol-level correctness (no timing)
//     Controller → progress tracking + timeout evaluation
//     Session    → lifecycle decisions (shutdown, draining)
//
// ---------------------------------------------------------------------------
// Design goals
// ---------------------------------------------------------------------------
// • Compile-time injectable
// • Zero runtime branching (via if constexpr)
// • No state
// • Deterministic behavior
// • Ultra-low-latency friendly
//
// ---------------------------------------------------------------------------
// Semantics
// ---------------------------------------------------------------------------
//
// Strict
//   - Pure correctness mode
//   - Requires full convergence:
//         all pending work must be ACKed or rejected
//   - No timeout fallback
//
// Timeout<N>
//   - Bounded waiting mode
//   - If no progress is observed for N nanoseconds,
//         the system is considered idle
//
//   - N is a compile-time constant, allowing fine-grained tuning:
//
//         Timeout<5min>   → exchange-safe default
//         Timeout<1s>     → low-latency trading
//         Timeout<100ms>  → ultra-aggressive strategies
//
// ---------------------------------------------------------------------------
// Progress definition
// ---------------------------------------------------------------------------
//
// "Progress" is defined as any observable state transition:
//
//   • register_subscription / register_unsubscription
//   • ACK processing (subscribe / unsubscribe)
//   • rejection handling
//   • state reset (clear_all)
//
// The policy does NOT decide what progress is —
// it only defines how long we tolerate the absence of it.
//
// ---------------------------------------------------------------------------
// Notes
// ---------------------------------------------------------------------------
//
// • This policy is purely declarative (no runtime logic)
// • Timeout behavior is implemented by the Controller
// • Session uses Controller signals to drive lifecycle decisions
//
// ============================================================================


// ----------------------------------------------------------------------------
// Strict (no timeout)
// ----------------------------------------------------------------------------

struct Strict {

    static constexpr bool enabled = false;
    static constexpr std::uint64_t timeout_ns = 0;

    static constexpr const char* mode_name() noexcept {
        return "Strict";
    }

    static void dump(std::ostream& os) {
        os << "[Protocol Progress Policy]\n";
        os << "- Mode        : Strict\n";
        os << "- Enabled     : no\n\n";
    }
};

static_assert(ProgressConcept<Strict>);


// ----------------------------------------------------------------------------
// Timeout (parameterized)
// ----------------------------------------------------------------------------

template<std::uint64_t TimeoutNs>
struct Timeout {

    static_assert(TimeoutNs > 0, "Timeout must be > 0");

    static constexpr bool enabled = true;
    static constexpr std::uint64_t timeout_ns = TimeoutNs;

    static constexpr const char* mode_name() noexcept {
        return "Timeout";
    }

    static void dump(std::ostream& os) {
        os << "[Protocol Progress Policy]\n";
        os << "- Mode        : Timeout\n";
        os << "- Enabled     : yes\n";
        os << "- Timeout     : " << lcr::format_duration(timeout_ns) << "\n\n";
    }
};

static_assert(ProgressConcept<Timeout<1>>);


// ----------------------------------------------------------------------------
// Convenience aliases (optional, but nice UX)
// ----------------------------------------------------------------------------

using RelaxedTimeout   = Timeout<300'000'000'000ULL>; // 5 min  (exchange-safe timeout)
using DefaultTimeout   = Timeout<30'000'000'000ULL>;  // 30 sec (reasonable timeout)
using FastTimeout      = Timeout<1'000'000'000ULL>;   // 1 sec  (low-latency timeout)
using StrictTimeout    = Timeout<100'000'000ULL>;     // 100 ms (aggressive timeout)


} // namespace progress


// ----------------------------------------------------------------------------
// Default
// ----------------------------------------------------------------------------

using DefaultProgress = progress::DefaultTimeout;

static_assert(ProgressConcept<DefaultProgress>);

} // namespace wirekrak::core::policy::protocol

#pragma once

// ============================================================================
// Protocol Symbol Limit Policy
// ============================================================================
//
// Controls compile-time subscription capacity limits enforced by the Session.
//
// Symbol limits allow the application to define deterministic bounds on the
// number of active market data subscriptions. This prevents accidental
// overload, protects system resources, and enforces exchange-imposed limits.
//
// The policy is evaluated before subscription requests are sent to the
// transport layer, ensuring that invalid requests are rejected locally and
// never reach the exchange.
//
// DESIGN GOALS
// ------------
// • Compile-time configurable limits
// • Deterministic enforcement
// • Zero runtime overhead when disabled
// • No dynamic allocation
// • Protocol correctness preserved
//
// POLICY MODES
// ------------
//
// None
//   - No subscription limits are enforced.
//   - The Session forwards all subscription requests to the transport.
//
// Hard
//   - Subscription requests exceeding configured limits are rejected
//     immediately by the Session.
//   - No transport message is sent.
//   - No partial allocation occurs.
//   - Replay database and managers remain unchanged.
//
// LIMIT TYPES
// -----------
//
// MaxTrade
//   Maximum number of trade channel symbols.
//
// MaxBook
//   Maximum number of book channel symbols.
//
// MaxGlobal
//   Maximum total number of symbols across all channels.
//
// The global limit must always be greater than or equal to the per-channel
// limits to maintain internal consistency.
//
// EXAMPLE
// -------
//
//   SymbolLimitPolicy<LimitMode::Hard, 100, 50, 120>
//
//   - Up to 100 trade symbols allowed
//   - Up to 50 book symbols allowed
//   - Up to 120 symbols total across all channels
//
// USE CASES
// ---------
// • Enforcing exchange subscription caps
// • Protecting systems from excessive subscription fan-out
// • Risk-controlled deployments
// • Multi-tenant environments with bounded capacity
//
// ============================================================================

#include <cstddef>
#include <concepts>
#include <ostream>


namespace wirekrak::core::policy::protocol {

// ------------------------------------------------------------
// Limit Mode
// ------------------------------------------------------------

enum class LimitMode {
    None,   // No limits enforced
    Hard    // Reject when exceeding limit
};


// ------------------------------------------------------------
// Concept
// ------------------------------------------------------------

template<class T>
concept HasSymbolLimitMembers =
    requires {
        { T::mode } -> std::same_as<const LimitMode&>;
        { T::max_trade } -> std::same_as<const std::size_t&>;
        { T::max_book } -> std::same_as<const std::size_t&>;
        { T::max_global } -> std::same_as<const std::size_t&>;
        { T::enabled } -> std::same_as<const bool&>;
        { T::hard } -> std::same_as<const bool&>;
    };

template<class T>
concept SymbolLimitConcept =
    HasSymbolLimitMembers<T>
    &&
    (
        T::enabled == (T::mode != LimitMode::None)
    )
    &&
    (
        T::hard == (T::mode == LimitMode::Hard)
    )
    &&
    (
        // Mode None → all limits must be zero
        (
            T::mode == LimitMode::None &&
            T::max_trade == 0 &&
            T::max_book == 0 &&
            T::max_global == 0
        )
        ||
        // Mode Hard → global must be consistent
        (
            T::mode == LimitMode::Hard &&
            T::max_global >= T::max_trade &&
            T::max_global >= T::max_book
        )
    );


// ------------------------------------------------------------
// SymbolLimitPolicy
// ------------------------------------------------------------
//
// Compile-time subscription symbol limits.
//
// Template parameters:
//   ModeV       → None or Hard
//   MaxTradeV   → Maximum trade symbols
//   MaxBookV    → Maximum book symbols
//   MaxGlobalV  → Maximum total symbols across channels
//
// All limits are enforced at Session level.
// Manager and ReplayDB remain policy-agnostic.
//
// ------------------------------------------------------------

template<
    LimitMode ModeV,
    std::size_t MaxTradeV,
    std::size_t MaxBookV,
    std::size_t MaxGlobalV
>
struct SymbolLimitPolicy {

    static constexpr LimitMode mode = ModeV;

    static constexpr std::size_t max_trade  = MaxTradeV;
    static constexpr std::size_t max_book   = MaxBookV;
    static constexpr std::size_t max_global = MaxGlobalV;

    static constexpr bool enabled =  ModeV != LimitMode::None;

    static constexpr bool hard = ModeV == LimitMode::Hard;

    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
    // ------------------------------------------------------------
    //
    // These helpers allow compile-time policies to expose
    // their configuration in a deterministic, presentation-friendly way.
    //
    // - No instances required
    // - No dynamic allocation
    // - Fully inlinable
    // - Removed entirely by the compiler if unused
    //

    // Human-readable mode name
    static constexpr const char* mode_name() noexcept {
        switch (mode) {
            case LimitMode::None: return "None";
            case LimitMode::Hard: return "Hard";
        }
        return "Unknown"; // defensive fallback (should never happen)
    }

    // Dump policy configuration
    static void dump(std::ostream& os) {
        os << "[Protocol Symbol Limit Policy]\n";

        os << "- Mode        : " << mode_name() << "\n";
        os << "- Enabled     : " << (enabled ? "yes" : "no") << "\n";

        if constexpr (ModeV == LimitMode::Hard) {
            os << "- Max trade   : " << max_trade  << "\n";
            os << "- Max book    : " << max_book   << "\n";
            os << "- Max global  : " << max_global << "\n";
        }

        os << "\n";
    }
};


// ------------------------------------------------------------
// Predefined Policies
// ------------------------------------------------------------

using NoSymbolLimits =
    SymbolLimitPolicy<
        LimitMode::None,
        0, 0, 0
    >;

// Assert that NoSymbolLimits satisfies the SymbolLimitConcept
static_assert(SymbolLimitConcept<NoSymbolLimits>, "NoSymbolLimits does not satisfy SymbolLimitConcept");

// ----------------------------------------------------------------------------
// Default
// ----------------------------------------------------------------------------

using DefaultSymbolLimit = NoSymbolLimits;

static_assert(SymbolLimitConcept<DefaultSymbolLimit>, "DefaultSymbolLimit does not satisfy SymbolLimitConcept");

} // namespace wirekrak::core::policy::protocol

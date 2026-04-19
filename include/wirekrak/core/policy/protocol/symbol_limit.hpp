#pragma once

// ============================================================================
// Protocol Symbol Limit Policy (Generic, Exchange-Agnostic)
// ============================================================================
//
// Controls compile-time subscription capacity limits enforced by the Session.
//
// This version is fully protocol-agnostic and does NOT assume any notion of
// channels (e.g. trade, book). Limits are expressed in terms of:
//
//   • max_per_channel  → maximum symbols per single channel
//   • max_global       → maximum total active symbols
//
// DESIGN GOALS
// ------------
// • Compile-time configurable limits
// • Deterministic enforcement
// • Zero runtime overhead when disabled
// • Fully exchange-agnostic
// • Backward-compatible extension path (per-type limits)
//
// POLICY MODES
// ------------
//
// None
//   - No limits enforced
//
// Hard
//   - Requests exceeding limits are rejected locally
//   - No transport emission
//   - No partial allocation
//
// EXTENSIBILITY
// -------------
//
// Future extensions may introduce:
//
//   template<class RequestT>
//   static constexpr std::size_t limit_for = max_per_channel;
//
// without breaking existing users.
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
    None,
    Hard
};

// ------------------------------------------------------------
// Concept
// ------------------------------------------------------------

template<class T>
concept HasSymbolLimitMembers =
    requires {
        { T::mode } -> std::same_as<const LimitMode&>;
        { T::max_per_channel } -> std::same_as<const std::size_t&>;
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
        // Mode None → no limits
        (
            T::mode == LimitMode::None &&
            T::max_per_channel == 0 &&
            T::max_global == 0
        )
        ||
        // Mode Hard → global must be >= per-request
        (
            T::mode == LimitMode::Hard &&
            T::max_global >= T::max_per_channel
        )
    );

// ------------------------------------------------------------
// SymbolLimitPolicy
// ------------------------------------------------------------

template<
    LimitMode ModeV,
    std::size_t MaxPerChannelV,
    std::size_t MaxGlobalV
>
struct SymbolLimitPolicy {

    static constexpr LimitMode mode = ModeV;

    static constexpr std::size_t max_per_channel = MaxPerChannelV;
    static constexpr std::size_t max_global      = MaxGlobalV;

    static constexpr bool enabled = ModeV != LimitMode::None;
    static constexpr bool hard    = ModeV == LimitMode::Hard;

    // ------------------------------------------------------------
    // FUTURE EXTENSION POINT (do not remove)
    // ------------------------------------------------------------
    //
    // Allows seamless upgrade to per-type limits:
    //
    // template<class RequestT>
    // static constexpr std::size_t limit_for = max_per_channel;
    //
    // ------------------------------------------------------------

    // ------------------------------------------------------------
    // Introspection
    // ------------------------------------------------------------

    static constexpr const char* mode_name() noexcept {
        switch (mode) {
            case LimitMode::None: return "None";
            case LimitMode::Hard: return "Hard";
        }
        return "Unknown";
    }

    static void dump(std::ostream& os) {
        os << "[Protocol Symbol Limit Policy]\n";

        os << "- Mode        : " << mode_name() << "\n";
        os << "- Enabled     : " << (enabled ? "yes" : "no") << "\n";

        if constexpr (ModeV == LimitMode::Hard) {
            os << "- Max/channel : " << max_per_channel << "\n";
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
        0,
        0
    >;

static_assert(SymbolLimitConcept<NoSymbolLimits>);

// ----------------------------------------------------------------------------
// Default
// ----------------------------------------------------------------------------

using DefaultSymbolLimit = NoSymbolLimits;

static_assert(SymbolLimitConcept<DefaultSymbolLimit>);

} // namespace wirekrak::core::policy::protocol

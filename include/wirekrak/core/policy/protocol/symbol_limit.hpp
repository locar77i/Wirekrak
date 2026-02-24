#pragma once

#include <cstddef>
#include <concepts>


namespace wirekrak::core::policy::protocol {

// ------------------------------------------------------------
// Limit Mode
// ------------------------------------------------------------

enum class LimitMode {
    None,   // No limits enforced
    Hard    // Reject when exceeding limit
};


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
};


// ------------------------------------------------------------
// Predefined Policies
// ------------------------------------------------------------

using NoSymbolLimits =
    SymbolLimitPolicy<
        LimitMode::None,
        0, 0, 0
    >;

// Example: max 16 trades, 16 books, 32 total
using Hard16 =
    SymbolLimitPolicy<
        LimitMode::Hard,
        16, 16, 32
    >;

// Example: asymmetric
using HardTrade32Book8 =
    SymbolLimitPolicy<
        LimitMode::Hard,
        32, 8, 32
    >;


// ------------------------------------------------------------
// Concept
// ------------------------------------------------------------

template<class T>
concept SymbolLimitConcept =
    requires {
        { T::mode } -> std::convertible_to<LimitMode>;
        { T::max_trade } -> std::convertible_to<std::size_t>;
        { T::max_book } -> std::convertible_to<std::size_t>;
        { T::max_global } -> std::convertible_to<std::size_t>;
        { T::enabled } -> std::convertible_to<bool>;
        { T::hard } -> std::convertible_to<bool>;
    };

} // namespace wirekrak::core::policy::protocol

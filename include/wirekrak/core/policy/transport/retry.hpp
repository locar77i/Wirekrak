#pragma once

/*
===============================================================================
 Transport Retry Policy
===============================================================================

Defines reconnection mode for transport::Connection.

Design goals:

• Compile-time injectable
• Zero runtime polymorphism
• No dynamic configuration
• Deterministic per Connection type
• Policy is NOT aware of transport errors

Retry decisions are controlled through a mode enum.

The connection implementation decides whether an error is retryable.

-------------------------------------------------------------------------------
 Retry Strategies
-------------------------------------------------------------------------------

Never
    No retry is ever performed.

RetryableOnly
    Retry only when the connection class marks the error as retryable.

Always
    Retry for any error.

-------------------------------------------------------------------------------
 Timing Profiles
-------------------------------------------------------------------------------

Three retry timing classes are defined:

fast    → used for temporary disconnections
normal  → used for handshake / network failures
slow    → used for severe transport failures

The connection chooses which profile to use.

-------------------------------------------------------------------------------
 Design Guarantees
-------------------------------------------------------------------------------

• Fully compile-time configuration
• Zero runtime overhead
• Deterministic behavior
• Clean separation of concerns
• Retry policy independent of error system

===============================================================================
*/

#include <chrono>
#include <concepts>
#include <ostream>

#include "wirekrak/core/policy/retry_mode.hpp"


namespace wirekrak::core::policy::transport {

// ============================================================================
// Retry Policy Concept
// ============================================================================

template<typename P>
concept HasRetryMembers =
requires {
    { P::mode } -> std::same_as<const RetryMode&>;

    { P::max_attempts } -> std::same_as<const std::uint32_t&>;
    { P::max_exponent } -> std::same_as<const std::uint32_t&>;

    { P::fast_base }   -> std::convertible_to<std::chrono::milliseconds>;
    { P::normal_base } -> std::convertible_to<std::chrono::milliseconds>;
    { P::slow_base }   -> std::convertible_to<std::chrono::milliseconds>;

    { P::fast_cap }   -> std::convertible_to<std::chrono::milliseconds>;
    { P::normal_cap } -> std::convertible_to<std::chrono::milliseconds>;
    { P::slow_cap }   -> std::convertible_to<std::chrono::milliseconds>;
};

template<typename P>
concept RetryConcept =
    HasRetryMembers<P>
    &&
    (P::max_exponent > 0);


// ============================================================================
// Retry Policy Implementations
// ============================================================================

namespace retry {

// -----------------------------------------------------------------------------
// Disabled
// -----------------------------------------------------------------------------

struct Disabled {

    static constexpr RetryMode mode = RetryMode::Never;

    static constexpr std::uint32_t max_attempts = 0;
    static constexpr std::uint32_t max_exponent = 1;

    static constexpr std::chrono::milliseconds fast_base{0};
    static constexpr std::chrono::milliseconds normal_base{0};
    static constexpr std::chrono::milliseconds slow_base{0};

    static constexpr std::chrono::milliseconds fast_cap{0};
    static constexpr std::chrono::milliseconds normal_cap{0};
    static constexpr std::chrono::milliseconds slow_cap{0};

    static constexpr const char* mode_name() noexcept {
        return "Never";
    }

    static void dump(std::ostream& os) {
        os << "[Transport Retry Policy]\n";
        os << "- Mode : " << mode_name() << "\n";
        os << "- Enabled  : no\n\n";
    }
};

static_assert(RetryConcept<Disabled>, "retry::Disabled does not satisfy RetryConcept");


// -----------------------------------------------------------------------------
// Exponential Retry (Default)
// -----------------------------------------------------------------------------

template<
    RetryMode Mode = RetryMode::RetryableOnly,
    std::uint32_t MaxExponent = 6,
    std::uint32_t MaxAttempts = 0   // 0 = infinite
>
struct Exponential {

    static constexpr RetryMode mode = Mode;

    static constexpr std::uint32_t max_attempts = MaxAttempts;
    static constexpr std::uint32_t max_exponent = MaxExponent;

    static constexpr std::chrono::milliseconds fast_base{50};
    static constexpr std::chrono::milliseconds normal_base{100};
    static constexpr std::chrono::milliseconds slow_base{250};

    static constexpr std::chrono::milliseconds fast_cap{1000};
    static constexpr std::chrono::milliseconds normal_cap{5000};
    static constexpr std::chrono::milliseconds slow_cap{10000};

    static constexpr const char* mode_name() noexcept {
        switch (mode) {
            case RetryMode::Never: return "Never";
            case RetryMode::RetryableOnly: return "RetryableOnly";
            case RetryMode::Always: return "Always";
        }
        return "Unknown";
    }

    // -------------------------------------------------------------------------
    // Backoff computation
    // -------------------------------------------------------------------------

    static constexpr std::chrono::milliseconds
    compute_backoff(std::chrono::milliseconds base, std::chrono::milliseconds cap, std::uint32_t attempt) noexcept {
        // Clamp exponent according to policy
        attempt = (attempt > max_exponent) ? max_exponent : attempt;
        // Exponential backoff with capping
        auto delay = base * (1u << attempt);
        return delay > cap ? cap : delay;
    }

    static void dump(std::ostream& os) {

        os << "[Transport Retry Policy]\n";
        os << "- Mode      : " << mode_name() << "\n";
        os << "- Max Exponent  : " << max_exponent << "\n";

        if constexpr (MaxAttempts == 0)
            os << "- Max Attempts  : infinite\n";
        else
            os << "- Max Attempts  : " << MaxAttempts << "\n";

        os << "- Fast Base     : " << fast_base.count() << " ms\n";
        os << "- Normal Base   : " << normal_base.count() << " ms\n";
        os << "- Slow Base     : " << slow_base.count() << " ms\n\n";
    }
};

static_assert(RetryConcept<Exponential<>>, "retry::Exponential does not satisfy RetryConcept"); 

} // namespace retry


// ============================================================================
// Default Retry Policy
// ============================================================================

using DefaultRetry = retry::Exponential<>;

static_assert(RetryConcept<DefaultRetry>, "DefaultRetry does not satisfy RetryConcept");

} // namespace wirekrak::core::policy::transport

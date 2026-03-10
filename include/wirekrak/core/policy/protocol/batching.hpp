#pragma once

// ============================================================================
// Protocol Batching Policy
// ============================================================================
//
// Controls how large subscription requests are emitted by the Session.
//
// Exchanges often return large snapshot bursts when many symbols are
// subscribed simultaneously. This policy allows the Session to split
// large subscription requests and optionally pace their emission.
//
// DESIGN GOALS
// ------------
// • Compile-time configurable
// • Zero runtime overhead when disabled
// • Deterministic behavior
// • No dynamic allocation in the policy itself
// • Session-controlled execution
//
// POLICY MODES
// ------------
//
// Immediate
//   - Subscriptions are sent exactly as requested
//   - No request splitting
//
// Batch
//   - Large subscriptions are split into multiple requests
//   - All batches are emitted in the same poll() iteration
//
// Paced
//   - Large subscriptions are split into batches
//   - One batch is emitted every N poll() calls
//
// EXAMPLE
// -------
//
//   User request:
//       subscribe(book, 200 symbols)
//
//   Batch mode (batch_size = 20):
//
//       poll 1 → send 10 requests:
//
//           subscribe(book, 20)
//           subscribe(book, 20)
//           subscribe(book, 20)
//           ...
//
//   Paced mode (batch_size = 20, emit_interval = 2):
//
//       poll 1 → subscribe(book, 20)
//       poll 3 → subscribe(book, 20)
//       poll 5 → subscribe(book, 20)
//
// USE CASES
// ---------
// • Large initial market data bootstraps
// • Exchanges with per-request symbol limits
// • Controlling subscription fan-out behavior
// • Smoothing snapshot bursts
//
// ============================================================================

#include <cstddef>
#include <concepts>
#include <ostream>

namespace wirekrak::core::policy::protocol {

// ------------------------------------------------------------
// Batching Mode
// ------------------------------------------------------------

enum class BatchingMode {
    Immediate,
    Batch,
    Paced
};


// ------------------------------------------------------------
// Concept Helpers
// ------------------------------------------------------------

template<class T>
concept HasBatchingMembers =
    requires {
        { T::mode } -> std::same_as<const BatchingMode&>;
        { T::batch_size } -> std::same_as<const std::size_t&>;
        { T::emit_interval } -> std::same_as<const std::size_t&>;
        { T::enabled } -> std::same_as<const bool&>;
    };


// ------------------------------------------------------------
// Batching Concept
// ------------------------------------------------------------

template<class T>
concept BatchingConcept =
    HasBatchingMembers<T>
    &&
    (
        T::enabled == (T::mode != BatchingMode::Immediate)
    )
    &&
    (
        // Immediate
        (
            T::mode == BatchingMode::Immediate &&
            T::batch_size == 0 &&
            T::emit_interval == 0
        )
        ||
        // Batch
        (
            T::mode == BatchingMode::Batch &&
            T::batch_size > 0 &&
            T::emit_interval == 0
        )
        ||
        // Paced
        (
            T::mode == BatchingMode::Paced &&
            T::batch_size > 0 &&
            T::emit_interval > 0
        )
    );


// ------------------------------------------------------------
// Batching Policy
// ------------------------------------------------------------

template<
    BatchingMode ModeV,
    std::size_t BatchSizeV = 0,
    std::size_t EmitIntervalV = 0
>
struct BatchingPolicy {

    static constexpr BatchingMode mode = ModeV;

    static constexpr std::size_t batch_size = BatchSizeV;

    // Used only for Paced mode
    static constexpr std::size_t emit_interval = EmitIntervalV;

    static constexpr bool enabled = ModeV != BatchingMode::Immediate;


    // ------------------------------------------------------------
    // Introspection Helpers
    // ------------------------------------------------------------

    static constexpr const char* mode_name() noexcept {
        switch (mode) {
            case BatchingMode::Immediate: return "Immediate";
            case BatchingMode::Batch:     return "Batch";
            case BatchingMode::Paced:     return "Paced";
        }
        return "Unknown";
    }

    static void dump(std::ostream& os) {

        os << "[Protocol Batching Policy]\n";

        os << "- Mode       : " << mode_name() << "\n";
        os << "- Enabled    : " << (enabled ? "yes" : "no") << "\n";

        if constexpr (ModeV != BatchingMode::Immediate) {
            os << "- Batch size : " << batch_size << "\n";
        }

        if constexpr (ModeV == BatchingMode::Paced) {
            os << "- Emit every : " << emit_interval << " poll(s)\n";
        }

        os << "\n";
    }
};


// ------------------------------------------------------------
// Predefined Policies
// ------------------------------------------------------------

using ImmediateBatching =
    BatchingPolicy<
        BatchingMode::Immediate
    >;

static_assert(BatchingConcept<ImmediateBatching>, "ImmediateBatching does not satisfy BatchingConcept");


// ------------------------------------------------------------
// Default
// ------------------------------------------------------------

using DefaultBatching = ImmediateBatching;

static_assert(BatchingConcept<DefaultBatching>, "DefaultBatching does not satisfy BatchingConcept");

} // namespace wirekrak::core::policy::protocol

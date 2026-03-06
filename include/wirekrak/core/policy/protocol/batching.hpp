#pragma once

// ============================================================================
// Protocol Batching Policy
// ============================================================================
//
// Controls how subscription requests are emitted by the Session when
// multiple symbols are requested at once.
//
// Large subscriptions (e.g. hundreds of symbols) may cause a burst of
// snapshot messages from the exchange. This burst can temporarily stress
// transport buffers, message rings, and user processing pipelines.
//
// The batching policy allows the Session to partition and schedule
// subscription requests in order to smooth the resulting snapshot load.
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
//   - Default behavior
//   - Subscriptions are sent exactly as requested
//   - Lowest latency, but may trigger large snapshot bursts
//
// Batch
//   - Large subscriptions are split into multiple requests
//   - All batches are sent immediately
//   - Reduces per-request snapshot fan-out while preserving low latency
//
// Paced
//   - Large subscriptions are split into batches
//   - Batches are emitted gradually by the Session event loop
//   - Smooths snapshot bursts and reduces memory pressure
//
// EXAMPLE
// -------
//
//   User request:
//       subscribe(book, 200 symbols)
//
//   Batch mode (batch_size = 20):
//
//       subscribe(book, 20)
//       subscribe(book, 20)
//       subscribe(book, 20)
//       ...
//
//   Paced mode (batch_size = 20, pacing_interval = 1):
//
//       poll 1 → subscribe(book, 20)
//       poll 2 → subscribe(book, 20)
//       poll 3 → subscribe(book, 20)
//
// USE CASES
// ---------
// • Large initial market data bootstraps
// • Exchanges with per-request symbol limits
// • Reducing snapshot bursts and memory pressure
// • Controlling subscription fan-out behavior
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
    Immediate,  // Send subscriptions immediately (no batching)
    Batch,      // Split subscription into batches and send immediately
    Paced       // Split subscription and send batches gradually
};


// ------------------------------------------------------------
// Concept
// ------------------------------------------------------------

template<class T>
concept HasBatchingMembers =
    requires {
        { T::mode } -> std::same_as<const BatchingMode&>;
        { T::batch_size } -> std::same_as<const std::size_t&>;
        { T::pacing_interval } -> std::same_as<const std::size_t&>;
        { T::enabled } -> std::same_as<const bool&>;
        { T::paced } -> std::same_as<const bool&>;
    };

template<class T>
concept BatchingConcept =
    HasBatchingMembers<T>
    &&
    (
        T::enabled == (T::mode != BatchingMode::Immediate)
    )
    &&
    (
        T::paced == (T::mode == BatchingMode::Paced)
    )
    &&
    (
        // Immediate → no batching parameters
        (
            T::mode == BatchingMode::Immediate &&
            T::batch_size == 0 &&
            T::pacing_interval == 0
        )
        ||
        // Batch → batch size must be > 0, no pacing
        (
            T::mode == BatchingMode::Batch &&
            T::batch_size > 0 &&
            T::pacing_interval == 0
        )
        ||
        // Paced → both parameters required
        (
            T::mode == BatchingMode::Paced &&
            T::batch_size > 0 &&
            T::pacing_interval > 0
        )
    );


// ------------------------------------------------------------
// BatchingPolicy
// ------------------------------------------------------------
//
// Compile-time subscription batching behavior.
//
// Template parameters:
//   ModeV           → Immediate / Batch / Paced
//   BatchSizeV      → Symbols per subscription request
//   PacingIntervalV → Poll cycles between batch sends (Paced mode)
//
// Behavior:
//
// Immediate
//   - Send subscriptions exactly as requested
//
// Batch
//   - Split subscriptions into batches of BatchSize
//   - Send all batches immediately
//
// Paced
//   - Split subscriptions into batches
//   - Send gradually based on pacing interval
//
// ------------------------------------------------------------

template<
    BatchingMode ModeV,
    std::size_t BatchSizeV,
    std::size_t PacingIntervalV
>
struct BatchingPolicy {

    static constexpr BatchingMode mode = ModeV;

    static constexpr std::size_t batch_size = BatchSizeV;
    static constexpr std::size_t pacing_interval = PacingIntervalV;

    static constexpr bool enabled = ModeV != BatchingMode::Immediate;

    static constexpr bool paced = ModeV == BatchingMode::Paced;


    // ------------------------------------------------------------
    // Introspection Helpers (Zero Runtime Cost)
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

        os << "- Mode             : " << mode_name() << "\n";
        os << "- Enabled          : " << (enabled ? "yes" : "no") << "\n";

        if constexpr (ModeV != BatchingMode::Immediate) {
            os << "- Batch size       : " << batch_size << "\n";
        }

        if constexpr (ModeV == BatchingMode::Paced) {
            os << "- Pacing interval  : " << pacing_interval << " poll(s)\n";
        }

        os << "\n";
    }
};


// ------------------------------------------------------------
// Predefined Policies
// ------------------------------------------------------------

using ImmediateBatching =
    BatchingPolicy<
        BatchingMode::Immediate,
        0,
        0
    >;

static_assert(BatchingConcept<ImmediateBatching>, "ImmediateBatching does not satisfy BatchingConcept");


// ------------------------------------------------------------
// Default
// ------------------------------------------------------------

using DefaultBatching = ImmediateBatching;

static_assert(BatchingConcept<DefaultBatching>, "DefaultBatching does not satisfy BatchingConcept");

} // namespace wirekrak::core::policy::protocol

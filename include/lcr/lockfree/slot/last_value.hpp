/*
================================================================================
 last_value<T>
================================================================================

A lock-free, overwrite-on-write, single-writer / multi-reader storage primitive
for **state-like data** where *freshness matters more than history*.

This primitive intentionally does **not** queue values. Instead, each write
overwrites the previously stored value, and readers may observe only the most
recent update.

--------------------------------------------------------------------------------
 Design intent
--------------------------------------------------------------------------------

`last_value` is designed for control-plane and telemetry signals such as:

  • protocol liveness (pong, heartbeat)
  • session status
  • health indicators
  • last-seen metadata

It is *not* suitable for data streams where ordering or losslessness is required
(e.g. trades, order book updates, acknowledgements).

--------------------------------------------------------------------------------
 Concurrency model
--------------------------------------------------------------------------------

  • Single writer
      - Exactly one thread may call `store()`
      - No concurrent writers allowed

  • Multiple readers
      - Any number of threads may call `load_if_updated()`
      - Readers never modify shared state

  • Lock-free and wait-free
      - No locks, no CAS loops, no blocking
      - All operations are O(1) and noexcept

--------------------------------------------------------------------------------
 Semantics
--------------------------------------------------------------------------------

  • Overwrite-on-write
      - New writes always replace the previously stored value
      - Intermediate values may be lost intentionally

  • Epoch-based change detection
      - Each successful store increments a monotonically increasing epoch
      - Readers track the last observed epoch to detect updates

  • Pull-based observation
      - No callbacks, no observers, no reentrancy
      - Readers decide *when* to observe changes

--------------------------------------------------------------------------------
 Memory ordering
--------------------------------------------------------------------------------

  Memory safety relies on:

  • Writer performs:
        value_ write (non-atomic)
        epoch_.fetch_add(..., memory_order_release)

  • Reader performs:
        epoch_.load(memory_order_acquire)
        then reads value_

The release/acquire pair establishes a happens-before relationship,
making the non-atomic read of `value_` well-defined for trivially copyable types.

--------------------------------------------------------------------------------
 Limitations
--------------------------------------------------------------------------------

  • Only one writer is allowed
  • No history is preserved
  • The stored type `T` must be trivially copyable
      - No internal ownership (e.g. no std::string, std::vector, std::optional<std::string>)
      - No non-trivial destructor
      - Safe for non-atomic concurrent reads after release/acquire synchronization

  • Exactly one writer thread is allowed
  • No history is preserved

--------------------------------------------------------------------------------
 Summary
--------------------------------------------------------------------------------

`last_value` is the correct primitive when:

  • only the most recent value matters
  • loss of intermediate updates is acceptable
  • overwrite-on-full is desired behavior
  • strict reentrancy and determinism are required

  Epoch overflow is permitted and wraparound is benign; equality comparison is sufficient.

================================================================================
*/
#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace lcr {
namespace lockfree {
namespace slot {


template <typename T>
class alignas(64) last_value {
    static_assert(std::is_trivially_copyable_v<T>, "last_value<T> requires T to be trivially copyable");

public:
    last_value() noexcept = default;
    ~last_value() noexcept = default;

    // Non-copyable / non-movable
    last_value(const last_value&) = delete;
    last_value& operator=(const last_value&) = delete;

    // -------------------------------------------------------------------------
    // Writer API
    // -------------------------------------------------------------------------

    // Overwrite the stored value with the newest one.
    // Must be called by the single writer thread only.
    inline void store(T&& value) noexcept {
        value_ = std::move(value);
        epoch_.fetch_add(1, std::memory_order_release);
    }

    inline void store(const T& value) noexcept {
        value_ = value;
        epoch_.fetch_add(1, std::memory_order_release);
    }

    // -------------------------------------------------------------------------
    // Reader API
    // -------------------------------------------------------------------------

    // Load the stored value if it has changed since `last_epoch`.
    //
    // On success:
    //   - `out` is updated with the most recent value
    //   - `last_epoch` is updated to the current epoch
    //   - returns true
    //
    // If no update occurred:
    //   - `out` is left unchanged
    //   - returns false
    //
    [[nodiscard]]
    inline bool load_if_updated(T& out, std::uint64_t& last_epoch) const noexcept {
        const std::uint64_t e = epoch_.load(std::memory_order_acquire);
        if (e == last_epoch)
            return false;

        out = value_;
        last_epoch = e;
        return true;
    }


    // Convenience reader API for pull-based observation without explicit epoch
    // management.
    //
    // This method attempts to load the most recently stored value if it has changed
    // since the last successful call *on the current thread*. Change detection is
    // implemented via a thread-local epoch snapshot.
    //
    // Semantics:
    //   - Returns true and updates `out` if a newer value has been published.
    //   - Returns false if no update has occurred since the last call on this thread.
    //   - Intermediate updates may be skipped; only the most recent value is observed.
    //
    // Concurrency & ordering:
    //   - Safe for concurrent use by multiple reader threads.
    //   - Each reader thread tracks its own observation state independently.
    //   - No locks, no blocking, no reentrancy.
    //
    // Limitations:
    //   - Change detection is per-thread; calls from different threads do not share observation state.
    //   - Not suitable when every update must be observed.
    [[nodiscard]]
    inline bool try_load(T& out) const noexcept {
        static thread_local std::uint64_t last_epoch = 0;
        return load_if_updated(out, last_epoch);
    }

    // Return the current epoch.
    // Useful for manual change detection or polling strategies.
    [[nodiscard]]
    inline std::uint64_t epoch() const noexcept {
        return epoch_.load(std::memory_order_acquire);
    }

private:
    alignas(64) T value_{};
    alignas(64) std::atomic<std::uint64_t> epoch_{0};
};

} // namespace slot
} // namespace lockfree
} // namespace lcr

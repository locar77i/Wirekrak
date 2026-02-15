/*
================================================================================
 last_value_snapshot<T>
================================================================================

A lock-free, double-buffered, single-writer / multi-reader publication primitive
for **state-like data** where freshness matters more than history and the stored
type may be non-trivially copyable.

Unlike `last_value<T>`, this implementation safely supports complex
types (e.g. std::string, std::vector, protocol messages) without introducing
data races.

--------------------------------------------------------------------------------
 Design intent
--------------------------------------------------------------------------------

`last_value_snapshot` is designed for control-plane and metadata-style signals:

  • protocol pong messages
  • system status updates
  • health snapshots
  • last-seen structured state

It is NOT intended for ordered, lossless, or high-frequency data streams
(trades, order book deltas, acknowledgements).

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
      - No locks
      - No CAS loops
      - No blocking
      - O(1) operations

--------------------------------------------------------------------------------
 Design strategy
--------------------------------------------------------------------------------

This primitive uses double-buffered publication:

  • Two internal buffers are maintained.
  • The writer writes into the inactive buffer.
  • Once fully written, the writer publishes it via an atomic index flip.
  • Readers only access the currently published buffer.

This guarantees:

  • No reader ever observes a partially written object.
  • No concurrent read/write of the same object.
  • No data races on non-trivial types.

--------------------------------------------------------------------------------
 Memory ordering
--------------------------------------------------------------------------------

Writer:
    1) Writes value into inactive buffer (non-atomic)
    2) Publishes new buffer index with memory_order_release
    3) Increments epoch with memory_order_release

Reader:
    1) Loads epoch with memory_order_acquire
    2) Loads active index with memory_order_acquire
    3) Copies from the published buffer

Release/acquire establishes a happens-before relationship,
ensuring the published object is fully constructed before any
reader observes it.

--------------------------------------------------------------------------------
 Semantics
--------------------------------------------------------------------------------

  • Overwrite-on-write
      - Only the most recent value is retained
      - Intermediate updates may be skipped intentionally

  • Epoch-based change detection
      - Each store increments a monotonically increasing epoch
      - Readers detect updates by comparing epochs

  • Pull-based observation
      - No callbacks
      - No implicit dispatch
      - Readers explicitly poll for changes

--------------------------------------------------------------------------------
 Limitations
--------------------------------------------------------------------------------

  • Exactly one writer thread is allowed
  • No history is preserved
  • Copies occur on both store() and load()
  • Memory footprint is 2 × sizeof(T)

Epoch overflow is permitted; equality comparison is sufficient
for change detection.

--------------------------------------------------------------------------------
 Summary
--------------------------------------------------------------------------------

`last_value_snapshot<T>` is the correct primitive when:

  • the stored type is non-trivially copyable
  • overwrite semantics are desired
  • freshness matters more than completeness
  • deterministic, lock-free publication is required

================================================================================
*/

#pragma once

#include <atomic>
#include <cstdint>
#include <utility>
#include <type_traits>

namespace lcr::lockfree::slot {

template <typename T>
class alignas(64) last_value_snapshot {
public:
    last_value_snapshot() noexcept = default;
    ~last_value_snapshot() noexcept = default;

    last_value_snapshot(const last_value_snapshot&) = delete;
    last_value_snapshot& operator=(const last_value_snapshot&) = delete;

    // ============================================================
    // Writer API (single thread only)
    // ============================================================

    inline void store(const T& value) noexcept {
        publish_(value);
    }

    inline void store(T&& value) noexcept {
        publish_(std::move(value));
    }

    // ============================================================
    // Reader API (multi-thread safe)
    // ============================================================

    [[nodiscard]]
    inline bool load_if_updated(T& out, std::uint64_t& last_epoch) const noexcept {
        const auto e = epoch_.load(std::memory_order_acquire);
        if (e == last_epoch)
            return false;

        const auto idx = active_index_.load(std::memory_order_acquire);
        out = buffers_[idx];

        last_epoch = e;
        return true;
    }

    [[nodiscard]]
    inline bool try_load(T& out) const noexcept {
        static thread_local std::uint64_t last_epoch = 0;
        return load_if_updated(out, last_epoch);
    }

    [[nodiscard]]
    inline std::uint64_t epoch() const noexcept {
        return epoch_.load(std::memory_order_acquire);
    }

private:
    template<typename U>
    inline void publish_(U&& value) noexcept {
        const uint8_t current = active_index_.load(std::memory_order_relaxed);
        const uint8_t next = current ^ 1;

        buffers_[next] = std::forward<U>(value);

        active_index_.store(next, std::memory_order_release);
        epoch_.fetch_add(1, std::memory_order_release);
    }

private:
    alignas(64) T buffers_[2];
    alignas(64) std::atomic<uint8_t> active_index_{0};
    alignas(64) std::atomic<std::uint64_t> epoch_{0};
};

} // namespace lcr::lockfree::slot

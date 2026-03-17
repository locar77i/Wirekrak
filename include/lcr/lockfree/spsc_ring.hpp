#pragma once

/*
===============================================================================
SPSC Ring (Zero-Copy Two-Phase API)
===============================================================================

This file defines a high-performance Single-Producer / Single-Consumer (SPSC)
ring buffer with a two-phase (acquire/commit) API.

Purpose:
  - Enable zero-copy message passing
  - Minimize memory movement and latency
  - Support high-throughput data-plane workloads

Design principles:
  - Zero dynamic allocation (fixed-size, compile-time capacity)
  - Lock-free, wait-free operations
  - Explicit ownership and lifecycle control
  - Maximum performance with minimal abstraction overhead

API semantics (two-phase protocol):

  Producer:
    1) acquire_producer_slot()  → obtain writable slot
    2) write data into slot
    3) commit_producer_slot()   → publish to consumer

    or:
    - discard_producer_slot()   → cancel write

  Consumer:
    1) peek_consumer_slot()     → access readable slot
    2) process data in-place
    3) release_consumer_slot()  → free slot

Key properties:
  - Zero-copy (no intermediate buffers)
  - No implicit object construction/destruction during transfer
  - Full control over message lifecycle

Debug safety:
  - Enforces correct acquire/commit and peek/release pairing (debug builds)
  - Detects misuse such as double-acquire or missing release

Threading model:
  - Exactly one producer thread
  - Exactly one consumer thread

Constraints:
  - Capacity must be a power of two
  - Effective usable capacity is (Capacity - 1)

Relationship to spsc_queue:
  - spsc_ring  : zero-copy, maximum performance, manual control
  - spsc_queue : value-based, simpler but slightly higher overhead

Typical use cases:
  - Network transport pipelines
  - High-frequency trading (HFT) data paths
  - Large message or fragmented message assembly
  - Systems where copy avoidance is critical

===============================================================================
*/

#include "lcr/lockfree/spsc_core.hpp"


namespace lcr::lockfree {

template <typename T, size_t Capacity>
class alignas(64) spsc_ring : private spsc_core<T, Capacity> {
    using base = spsc_core<T, Capacity>;

#ifndef NDEBUG
    bool producer_slot_acquired_ = false;
    bool consumer_slot_acquired_ = false;
#endif

public:
    using base::capacity;
    using base::empty;
    using base::full;
    using base::used;
    using base::free_slots;
    using base::memory_usage;

    // -------------------------------------------------------------------------
    // Producer API (two-phase)
    // -------------------------------------------------------------------------

    [[nodiscard]] inline T* acquire_producer_slot() noexcept {
        const size_t head = base::head_.index.load(std::memory_order_relaxed);
        if (base::is_full(head)) [[unlikely]] {
            return nullptr;
        }

#ifndef NDEBUG
        LCR_ASSERT(!producer_slot_acquired_);
        producer_slot_acquired_ = true;
#endif

        return &base::buffer_[head];
    }

    inline void commit_producer_slot() noexcept {
#ifndef NDEBUG
        LCR_ASSERT(producer_slot_acquired_);
        producer_slot_acquired_ = false;
#endif

        const size_t head = base::head_.index.load(std::memory_order_relaxed);
        base::head_.index.store(base::next(head), std::memory_order_release);
    }

    inline void discard_producer_slot() noexcept {
#ifndef NDEBUG
        LCR_ASSERT(producer_slot_acquired_);
        producer_slot_acquired_ = false;
#endif
    }

    // -------------------------------------------------------------------------
    // Consumer API (two-phase)
    // -------------------------------------------------------------------------

    [[nodiscard]] inline T* peek_consumer_slot() noexcept {
        const size_t tail = base::tail_.index.load(std::memory_order_relaxed);
        if (base::is_empty(tail)) [[unlikely]] {
            return nullptr;
        }

#ifndef NDEBUG
        LCR_ASSERT(!consumer_slot_acquired_);
        consumer_slot_acquired_ = true;
#endif

        return &base::buffer_[tail];
    }

    inline void release_consumer_slot() noexcept {
#ifndef NDEBUG
        LCR_ASSERT(consumer_slot_acquired_);
        consumer_slot_acquired_ = false;
#endif

        const size_t tail = base::tail_.index.load(std::memory_order_relaxed);
        base::tail_.index.store(base::next(tail), std::memory_order_release);
    }

    // --------------------------------------------------------------------------
    // Lifecycle
    // --------------------------------------------------------------------------

    inline void clear() noexcept {
        base::clear();
    #ifndef NDEBUG
        producer_slot_acquired_ = false;
        consumer_slot_acquired_ = false;
    #endif
    }
};

} // namespace lcr::lockfree

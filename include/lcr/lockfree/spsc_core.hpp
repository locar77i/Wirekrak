#pragma once

/*
===============================================================================
SPSC Core (Internal Building Block)
===============================================================================

This file defines the low-level, reusable core for Single-Producer /
Single-Consumer (SPSC) ring-based data structures.

Purpose:
  - Provide the fundamental lock-free storage and indexing mechanics
  - Encapsulate memory layout, atomic coordination, and cache alignment
  - Serve as a base for higher-level APIs (queue and ring variants)

Design principles:
  - Zero dynamic allocation (fixed-size, compile-time capacity)
  - Wait-free operations (bounded, constant-time)
  - Cache-friendly layout (separated producer/consumer indices)
  - Strict separation of concerns (no API semantics here)

Responsibilities:
  - Manage the circular buffer storage
  - Maintain producer (head) and consumer (tail) indices
  - Enforce memory ordering guarantees (acquire/release)
  - Provide utility helpers (empty, full, used, free_slots)

Non-responsibilities:
  - Does NOT define push/pop semantics
  - Does NOT expose two-phase (zero-copy) APIs
  - Does NOT enforce usage contracts (debug invariants live above)

Usage:
  - Intended for private inheritance only
  - Not meant to be used directly by end users

Threading model:
  - Exactly one producer thread
  - Exactly one consumer thread
  - No internal synchronization beyond atomics

Notes:
  - Capacity must be a power of two
  - Effective usable capacity is (Capacity - 1)
  - Designed for ultra-low-latency / HFT workloads

===============================================================================
*/

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

#include "lcr/memory/footprint.hpp"
#include "lcr/trap.hpp"


namespace lcr::lockfree {

template <typename T, size_t Capacity>
class alignas(64) spsc_core {
    static_assert((Capacity >= 2) && ((Capacity & (Capacity - 1)) == 0), "Capacity must be power of two and >= 2");

    static_assert(std::is_trivially_destructible_v<T> || std::is_nothrow_destructible_v<T>, "ring element must be safely destructible");

protected:
    static constexpr size_t MASK = Capacity - 1;

    struct alignas(64) PaddedAtomic {
        std::atomic<size_t> index{0};
        // Thread-local cache (producer/consumer only). Safe due to SPSC model.
        // Used for producer/consumer local caching to minimize atomic reads
        // Cached remote index:
        // - In head_: caches TAIL (used by producer)
        // - In tail_: caches HEAD (used by consumer)
        mutable size_t cached_opposite_index{0};
    };
    static_assert(sizeof(PaddedAtomic) <= 64);

    alignas(64) std::array<T, Capacity> buffer_{};
    alignas(64) PaddedAtomic head_;
    alignas(64) PaddedAtomic tail_;

    // ---- helpers ----

    [[nodiscard]]
    static constexpr size_t next(size_t v) noexcept {
        return (v + 1) & MASK;
    }

    [[nodiscard]]
    inline bool is_full(size_t head) const noexcept {
        const size_t next_val = next(head);
        if (next_val == head_.cached_opposite_index) { // It looks full, but is it really?
            // Only if looks full do I need to read the tail atomic. If it doesn't look full, I can trust that there's at least one free slot without reading the atomic.
            head_.cached_opposite_index = tail_.index.load(std::memory_order_acquire);
            return next_val == head_.cached_opposite_index;
        }
        return false; // There is space, no need to read the atomic
    }

    [[nodiscard]]
    inline bool is_empty(size_t tail) const noexcept {
        if (tail == tail_.cached_opposite_index) { // It looks empty, but is it really?
            tail_.cached_opposite_index = head_.index.load(std::memory_order_acquire);
            return tail == tail_.cached_opposite_index;
        }
        return false; // There is data, no need to read the atomic
    }

    // Advance head by N slots (Producer)
    inline void advance_head(size_t n) noexcept {
        const size_t h = head_.index.load(std::memory_order_relaxed);
        head_.index.store((h + n) & MASK, std::memory_order_release);
    }

    // Advance tail by N slots (Consumer)
    inline void advance_tail(size_t n) noexcept {
        const size_t t = tail_.index.load(std::memory_order_relaxed);
        tail_.index.store((t + n) & MASK, std::memory_order_release);
    }

    // Producer side helper to check if there are at least N free slots
    [[nodiscard]]
    inline bool has_free_slots(size_t head, size_t n) const noexcept {
        // distance from head to cached tail (minus 1)
        size_t available = (head_.cached_opposite_index - head - 1) & MASK;
        if (available < n) {
            head_.cached_opposite_index = tail_.index.load(std::memory_order_acquire);
            available = (head_.cached_opposite_index - head - 1) & MASK;
        }

        return available >= n;
    }

    // Consumer side helper to check if there are at least N available items
    [[nodiscard]]
    inline bool has_available(size_t tail, size_t n) const noexcept {
        size_t available = (tail_.cached_opposite_index - tail) & MASK;

        if (available < n) {
            tail_.cached_opposite_index = head_.index.load(std::memory_order_acquire);
            available = (tail_.cached_opposite_index - tail) & MASK;
        }

        return available >= n;
    }

public:
    spsc_core() noexcept = default;
    ~spsc_core() noexcept = default;

    spsc_core(const spsc_core&) = delete;
    spsc_core& operator=(const spsc_core&) = delete;

    // ---- common queries ----

    [[nodiscard]]
    inline bool empty() const noexcept {
        return tail_.index.load(std::memory_order_acquire) ==
               head_.index.load(std::memory_order_acquire);
    }

    [[nodiscard]]
    inline bool full() const noexcept {
        const size_t head = head_.index.load(std::memory_order_relaxed);
        return next(head) == tail_.index.load(std::memory_order_acquire);
    }

    [[nodiscard]]
    constexpr size_t capacity() const noexcept {
        return Capacity - 1;
    }

    [[nodiscard]]
    inline size_t used() const noexcept {
        const size_t h = head_.index.load(std::memory_order_acquire);
        const size_t t = tail_.index.load(std::memory_order_acquire);
        return (h - t) & MASK;
    }

    [[nodiscard]]
    inline size_t free_slots() const noexcept {
        return Capacity - 1 - used();
    }

    // ---- lifecycle ----

    inline void clear() noexcept {
        tail_.index.store(0, std::memory_order_relaxed);
        head_.index.store(0, std::memory_order_release);
    }

    [[nodiscard]]
    inline memory::footprint memory_usage() const noexcept {
        return memory::footprint{
            .static_bytes = sizeof(spsc_core<T, Capacity>),
            .dynamic_bytes = 0
        };
    }
};

} // namespace lcr::lockfree

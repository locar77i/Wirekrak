#pragma once

/*
===============================================================================
SPSC Queue (Value-Based API)
===============================================================================

This file defines a Single-Producer / Single-Consumer (SPSC) queue with a
simple push/pop interface.

Purpose:
  - Provide a safe, easy-to-use FIFO abstraction
  - Hide low-level details of the underlying ring buffer
  - Offer value-based semantics (copy/move)

Design principles:
  - Zero dynamic allocation (fixed-size, compile-time capacity)
  - Lock-free, wait-free operations
  - Minimal API surface (push / pop / emplace)
  - Clear ownership and lifecycle semantics

API semantics:
  - push(): copies or moves an element into the queue
  - pop(): extracts and transfers ownership to the caller
  - No partial states or multi-step operations

Characteristics:
  - Simpler but slightly higher overhead than zero-copy ring
  - Suitable for control-plane messages or lightweight payloads
  - No manual slot management required

Threading model:
  - Exactly one producer thread
  - Exactly one consumer thread

Constraints:
  - Capacity must be a power of two
  - Effective usable capacity is (Capacity - 1)

Relationship to spsc_ring:
  - spsc_queue: value-based, safe, ergonomic
  - spsc_ring : zero-copy, manual, maximum performance

Typical use cases:
  - Control-plane signaling
  - Small message passing
  - User-facing buffers where simplicity matters

===============================================================================
*/

#include "lcr/lockfree/spsc_core.hpp"


namespace lcr::lockfree {

template <typename T, size_t Capacity>
class alignas(64) spsc_queue : private spsc_core<T, Capacity> {
    using base = spsc_core<T, Capacity>;

public:
    using base::capacity;
    using base::empty;
    using base::full;
    using base::used;
    using base::free_slots;
    using base::clear;
    using base::memory_usage;

    // ---- push ----

    [[nodiscard]] inline bool push(const T& item) noexcept {
        const size_t head = base::head_.index.load(std::memory_order_relaxed);
        if (base::is_full(head)) return false;

        base::buffer_[head] = item;
        base::head_.index.store(base::next(head), std::memory_order_release);
        return true;
    }

    [[nodiscard]] inline bool push(T&& item) noexcept {
        const size_t head = base::head_.index.load(std::memory_order_relaxed);
        if (base::is_full(head)) return false;

        base::buffer_[head] = std::move(item);
        base::head_.index.store(base::next(head), std::memory_order_release);
        return true;
    }

    template <typename... Args>
    [[nodiscard]] inline bool emplace_push(Args&&... args) noexcept {
        const size_t head = base::head_.index.load(std::memory_order_relaxed);
        if (base::is_full(head)) return false;

        base::buffer_[head] = T(std::forward<Args>(args)...);
        base::head_.index.store(base::next(head), std::memory_order_release);
        return true;
    }

    // ---- pop ----

    [[nodiscard]] inline bool pop(T& out) noexcept {
        const size_t tail = base::tail_.index.load(std::memory_order_relaxed);
        if (base::is_empty(tail)) return false;

        out = std::move(base::buffer_[tail]);
        base::tail_.index.store(base::next(tail), std::memory_order_release);
        return true;
    }
};

} // namespace lcr::lockfree

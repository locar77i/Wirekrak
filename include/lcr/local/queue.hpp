#pragma once

#include <array>
#include <cstddef>
#include <utility>
#include <type_traits>

#include "lcr/memory/footprint.hpp"

namespace lcr {
namespace local {

/*
===============================================================================
Local SPSC Queue (Single-thread, value-based API)
===============================================================================

Single-threaded circular buffer with push/pop semantics.

Purpose:
  - Provide a simple, deterministic FIFO container
  - Value-based semantics (copy/move)
  - No partial states or multi-step operations

Characteristics:
  - O(1) push/pop
  - No dynamic allocation
  - Power-of-two capacity
  - Cache-friendly layout

Threading:
  - NOT thread-safe (single-thread only)

Use cases:
  - Internal queues
  - Control-plane buffering
  - Lightweight pipelines

===============================================================================
*/

template <typename T, size_t Capacity>
class alignas(64) queue {
    static_assert((Capacity >= 2) && ((Capacity & (Capacity - 1)) == 0),
        "Capacity must be power of two and >= 2");

    static_assert(std::is_trivially_destructible_v<T> ||
                  std::is_nothrow_destructible_v<T>,
        "ring element must be safely destructible");

public:
    queue() noexcept = default;
    ~queue() noexcept = default;

    queue(const queue&) = delete;
    queue& operator=(const queue&) = delete;

    // -------------------------------------------------------------------------
    // Push
    // -------------------------------------------------------------------------

    [[nodiscard]] inline bool push(const T& item) noexcept {
        const size_t next = (head_ + 1) & MASK;
        if (next == tail_) [[unlikely]]
            return false;
        buffer_[head_] = item;
        head_ = next;
        return true;
    }

    [[nodiscard]] inline bool push(T&& item) noexcept {
        const size_t next = (head_ + 1) & MASK;
        if (next == tail_) [[unlikely]]
            return false;
        buffer_[head_] = std::move(item);
        head_ = next;
        return true;
    }

    template <typename... Args>
    [[nodiscard]] inline bool emplace_push(Args&&... args) noexcept {
        const size_t next = (head_ + 1) & MASK;
        if (next == tail_) [[unlikely]]
            return false;
        buffer_[head_] = T(std::forward<Args>(args)...);
        head_ = next;
        return true;
    }

    // -------------------------------------------------------------------------
    // Pop
    // -------------------------------------------------------------------------

    [[nodiscard]] inline bool pop(T& out) noexcept {
        if (tail_ == head_) [[unlikely]]
            return false;
        out = std::move(buffer_[tail_]);
        tail_ = (tail_ + 1) & MASK;
        return true;
    }

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    [[nodiscard]] inline bool empty() const noexcept {
        return head_ == tail_;
    }

    [[nodiscard]] inline bool full() const noexcept {
        return ((head_ + 1) & MASK) == tail_;
    }

    [[nodiscard]] constexpr size_t capacity() const noexcept {
        return Capacity - 1;
    }

    [[nodiscard]] inline size_t size() const noexcept {
        return (head_ - tail_) & MASK;
    }

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    inline void clear() noexcept {
        head_ = tail_ = 0;
    }

    [[nodiscard]]
    inline memory::footprint memory_usage() const noexcept {
        return {
            .static_bytes = sizeof(queue<T, Capacity>),
            .dynamic_bytes = 0
        };
    }

private:
    static constexpr size_t MASK = Capacity - 1;

    std::array<T, Capacity> buffer_{};
    size_t head_{0};
    size_t tail_{0};
};

} // namespace local
} // namespace lcr

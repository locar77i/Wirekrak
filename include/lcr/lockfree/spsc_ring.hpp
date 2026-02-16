// -----------------------------------------------------------------------------
// Ultra-low-latency SPSC ring buffer with compile-time capacity
// Lock-free, wait-free, cacheline-separated producer/consumer indices.
// No dynamic memory allocations, perfect for real-time or HFT workloads.
//
// Example:
//     spsc_ring<Event*, 1024> queue;
//     queue.push(ptr);
//     Event* ev;
//     queue.pop(ev);
//
// Notes:
//   - Capacity must be a power of two (compile-time check)
//   - Single Producer, Single Consumer only
//   - All operations O(1), noexcept
// -----------------------------------------------------------------------------
#pragma once


#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <type_traits>

#include "lcr/memory/footprint.hpp"


namespace lcr::lockfree {

template <typename T, size_t Capacity>
class alignas(64) spsc_ring {
    static_assert((Capacity >= 2) && ((Capacity & (Capacity - 1)) == 0),
                  "Capacity must be power of two and >= 2");

public:
    spsc_ring() noexcept = default;
    ~spsc_ring() noexcept = default;

    // Non-copyable / non-movable
    spsc_ring(const spsc_ring&) = delete;
    spsc_ring& operator=(const spsc_ring&) = delete;

    // Push (copy)
    [[nodiscard]] inline bool push(const T& item) noexcept {
        const size_t head = head_.index.load(std::memory_order_relaxed);
        const size_t next = (head + 1) & MASK;
        if (next == tail_.index.load(std::memory_order_acquire))
            return false; // full
        buffer_[head] = item;
        head_.index.store(next, std::memory_order_release);
        return true;
    }

    // Push (move)
    [[nodiscard]] inline bool push(T&& item) noexcept {
        const size_t head = head_.index.load(std::memory_order_relaxed);
        const size_t next = (head + 1) & MASK;
        if (next == tail_.index.load(std::memory_order_acquire))
            return false; // full
        buffer_[head] = std::move(item);
        head_.index.store(next, std::memory_order_release);
        return true;
    }

    // Emplace (construct in-place)
    template <typename... Args>
    [[nodiscard]] inline bool emplace_push(Args&&... args) noexcept {
        const size_t head = head_.index.load(std::memory_order_relaxed);
        const size_t next = (head + 1) & MASK;
        if (next == tail_.index.load(std::memory_order_acquire))
            return false; // full
        buffer_[head] = T(std::forward<Args>(args)...);
        head_.index.store(next, std::memory_order_release);
        return true;
    }

    // Pop (move)
    [[nodiscard]] inline bool pop(T& out) noexcept {
        const size_t tail = tail_.index.load(std::memory_order_relaxed);
        if (tail == head_.index.load(std::memory_order_acquire))
            return false; // empty
        out = std::move(buffer_[tail]);
        tail_.index.store((tail + 1) & MASK, std::memory_order_release);
        return true;
    }

    [[nodiscard]] inline bool empty() const noexcept {
        return tail_.index.load(std::memory_order_acquire) ==
               head_.index.load(std::memory_order_acquire);
    }

    [[nodiscard]] inline bool full() const noexcept {
        const size_t next = (head_.index.load(std::memory_order_relaxed) + 1) & MASK;
        return next == tail_.index.load(std::memory_order_acquire);
    }

    [[nodiscard]] inline constexpr size_t capacity() const noexcept { return Capacity; }

    [[nodiscard]] inline size_t used() const noexcept {
        const size_t h = head_.index.load(std::memory_order_acquire);
        const size_t t = tail_.index.load(std::memory_order_acquire);
        return (h - t) & MASK;
    }

    [[nodiscard]] inline size_t free_slots() const noexcept {
        return Capacity - 1 - used();
    }

    [[nodiscard]] inline memory::footprint memory_usage() const noexcept {
       return memory::footprint{
            .static_bytes = sizeof(spsc_ring<T, Capacity>),
            .dynamic_bytes = 0
        };
    }

private:
    struct alignas(64) PaddedAtomic {
        std::atomic<size_t> index{0};
        char pad[64 - sizeof(std::atomic<size_t>)]{};
    };

    static constexpr size_t MASK = Capacity - 1;
    alignas(64) std::array<T, Capacity> buffer_{};
    alignas(64) PaddedAtomic head_;
    alignas(64) PaddedAtomic tail_;
};

} // namespace lcr::lockfree

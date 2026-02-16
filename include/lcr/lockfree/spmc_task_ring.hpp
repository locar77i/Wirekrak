// -----------------------------------------------------------------------------
// Ultra-Low-Latency SPMC Task Ring Buffer
// Single Producer → Multiple Consumers
// Each item is consumed exactly once by one consumer.
//
// Lock-free (consumers may retry under contention), no dynamic allocations.
// Designed for high-throughput task queues, schedulers, and ULL systems.
//
// Example:
//     spmc_task_ring<Task*, 1024> queue;
//     queue.push(task_ptr);
//     Task* task;
//     if (queue.pop(task)) { execute(task); }
//
// Notes:
//   - Capacity must be a power of two (compile-time check)
//   - Single producer, multiple consumers
//   - Each element consumed exactly once
//   - All operations O(1), noexcept
// -----------------------------------------------------------------------------
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <type_traits>


namespace lcr::lockfree {

template <typename T, size_t Capacity>
class alignas(64) spmc_task_ring {
    static_assert((Capacity >= 2) && ((Capacity & (Capacity - 1)) == 0),
                  "Capacity must be power of two and >= 2");

public:
    spmc_task_ring() noexcept = default;
    ~spmc_task_ring() noexcept = default;

    // Non-copyable / non-movable
    spmc_task_ring(const spmc_task_ring&) = delete;
    spmc_task_ring& operator=(const spmc_task_ring&) = delete;

    // Push (move)
    bool push(T&& item) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next = (head + 1) & MASK;

        const size_t tail = tail_.load(std::memory_order_acquire);
        if (next == tail)
            return false; // full

        buffer_[head] = std::move(item);
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Push (copy)
    bool push(const T& item) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next = (head + 1) & MASK;

        const size_t tail = tail_.load(std::memory_order_acquire);
        if (next == tail)
            return false; // full

        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Pop (move) - multiple consumers safely share this
    bool pop(T& out) noexcept {
        size_t tail = tail_.load(std::memory_order_relaxed);

        while (true) {
            const size_t head = head_.load(std::memory_order_acquire);
            if (tail == head)
                return false; // empty

            // Attempt to claim this slot (atomic fetch_add)
            if (tail_.compare_exchange_weak(
                    tail, (tail + 1) & MASK,
                    std::memory_order_acquire,
                    std::memory_order_relaxed))
            {
                out = std::move(buffer_[tail]);
                return true;
            }

            // CAS failed → another consumer took it → retry with updated tail
            // retry tail loaded in next loop iteration
        }
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    bool full() const noexcept {
        const size_t next = (head_.load(std::memory_order_relaxed) + 1) & MASK;
        const size_t tail = tail_.load(std::memory_order_acquire);
        return next == tail;
    }

    constexpr size_t capacity() const noexcept { return Capacity; }

    size_t size() const noexcept {
        const size_t h = head_.load(std::memory_order_acquire);
        const size_t t = tail_.load(std::memory_order_acquire);
        return (h >= t) ? (h - t) : (Capacity - (t - h));
    }

private:
    static constexpr size_t MASK = Capacity - 1;

    alignas(64) std::array<T, Capacity> buffer_;
    alignas(64) std::atomic<size_t> head_{0}; // written by producer
    alignas(64) std::atomic<size_t> tail_{0}; // shared by consumers
};

} // namespace lcr::lockfree

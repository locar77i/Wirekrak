#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <type_traits>

#include "lcr/memory/footprint.hpp"
#include "lcr/trap.hpp"


namespace lcr {
namespace local {

//------------------------------------------------------------------------------
// Single-threaded lock-free ring buffer for ultra-low-latency pipelines.
//
// A fixed-capacity, single-thread circular buffer optimized for deterministic
// performance, minimal cache footprint, and predictable access latency.
//
// Characteristics:
//   • O(1) push/pop operations (no dynamic allocations)
//   • Power-of-two capacity for modulo-free wraparound
//   • Cache-line aligned for predictable access patterns
//   • Suitable for:
//       - WAL management and rotation queues
//       - Internal batching / event pipelines
//       - Object pools or per-thread message buffers
//
// Thread-safety:
//   - NOT thread-safe. Must only be used from a single thread.
//   - For cross-thread communication, use the SPSC variant.
//
// Example:
//   ring<WalFile, 1024> wal_ring;
//   wal_ring.push(WalFile{"wal_0001.log", 1024 * 1024, 1});
//   WalFile oldest;
//   if (wal_ring.pop(oldest)) {
//       // process or delete oldest WAL
//   }
//
// Template parameters:
//   T         - element type stored in the buffer
//   Capacity  - must be a power of two and >= 2
//
// Notes:
//   Designed for performance-critical systems where predictable latency,
//   zero allocation, and cache efficiency are required (e.g. HFT, databases).
//------------------------------------------------------------------------------
template <typename T, size_t Capacity>
class alignas(64) ring {
    static_assert((Capacity >= 2) && ((Capacity & (Capacity - 1)) == 0), "Capacity must be power of two and >= 2");
    static_assert(std::is_trivially_destructible_v<T> || std::is_nothrow_destructible_v<T>, "ring element must be safely destructible");

public:
    ring() noexcept = default;
    ~ring() noexcept = default;

    // Non-copyable / non-movable
    ring(const ring&) = delete;
    ring& operator=(const ring&) = delete;

    // Push (copy)
    [[nodiscard]]
    inline bool push(const T& item) noexcept {
        const size_t next = (head_ + 1) & MASK;
        if (next == tail_) [[unlikely]]
            return false; // full
        buffer_[head_] = item;
        head_ = next;
        return true;
    }

    // Push (move)
    [[nodiscard]]
    inline bool push(T&& item) noexcept {
        const size_t next = (head_ + 1) & MASK;
        if (next == tail_) [[unlikely]]
            return false; // full
        buffer_[head_] = std::move(item);
        head_ = next;
        return true;
    }

    // Emplace
    template <typename... Args>
    [[nodiscard]]
    inline bool emplace_push(Args&&... args) noexcept {
        const size_t next = (head_ + 1) & MASK;
        if (next == tail_) [[unlikely]]
            return false;
        buffer_[head_] = T(std::forward<Args>(args)...);
        head_ = next;
        return true;
    }

    // Pop
    [[nodiscard]]
    inline bool pop(T& out) noexcept {
        if (tail_ == head_) [[unlikely]]
            return false; // empty
        out = std::move(buffer_[tail_]);
        tail_ = (tail_ + 1) & MASK;
        return true;
    }

    [[nodiscard]]
    inline bool empty() const noexcept {
        return head_ == tail_;
    }

    [[nodiscard]]
    inline bool full() const noexcept {
        return ((head_ + 1) & MASK) == tail_;
    }

    [[nodiscard]]
    inline constexpr size_t capacity() const noexcept {
        return Capacity - 1;
    }

    [[nodiscard]]
    inline size_t size() const noexcept {
        return (head_ - tail_) & MASK;
    }

    // -----------------------------------------------------------------------------
    // Zero-copy producer API (two-phase commit)
    // -----------------------------------------------------------------------------

    // Acquire writable slot
    [[nodiscard]]
    inline T* acquire_producer_slot() noexcept {
        const size_t next = (head_ + 1) & MASK;
        if (next == tail_) [[unlikely]] {
            return nullptr; // full
        }
#ifndef NDEBUG
        LCR_ASSERT(!producer_slot_acquired_); // double acquire
        producer_slot_acquired_ = true;
#endif
        return &buffer_[head_];
    }


    // Commit producer slot
    inline void commit_producer_slot() noexcept {
#ifndef NDEBUG
        LCR_ASSERT(producer_slot_acquired_); // commit without acquire
        producer_slot_acquired_ = false;
#endif
        head_ = (head_ + 1) & MASK;
    }


    // Discard producer slot
    inline void discard_producer_slot() noexcept {
#ifndef NDEBUG
        LCR_ASSERT(producer_slot_acquired_); // discard without acquire
        producer_slot_acquired_ = false;
#endif
    }

    // -----------------------------------------------------------------------------
    // Zero-copy consumer API (two-phase release)
    // -----------------------------------------------------------------------------

    // Peek readable slot
    [[nodiscard]]
    inline T* peek_consumer_slot() noexcept {
        if (tail_ == head_) [[unlikely]] {
            return nullptr; // empty
        }
#ifndef NDEBUG
        LCR_ASSERT(!consumer_slot_acquired_); // double peek
        consumer_slot_acquired_ = true;
#endif
        return &buffer_[tail_];
    }


    // Release consumed slot
    inline void release_consumer_slot() noexcept {
#ifndef NDEBUG
        LCR_ASSERT(consumer_slot_acquired_); // release without peek
        consumer_slot_acquired_ = false;
#endif
        tail_ = (tail_ + 1) & MASK;
    }

    // -----------------------------------------------------------------------------
    // Clear the ring (lifecycle operation)
    // -----------------------------------------------------------------------------
    inline void clear() noexcept {
        head_ = tail_ = 0;
#ifndef NDEBUG
        producer_slot_acquired_ = false;
        consumer_slot_acquired_ = false;
#endif
    }

    [[nodiscard]]
    inline memory::footprint memory_usage() const noexcept {
        return memory::footprint{
            .static_bytes = sizeof(ring<T, Capacity>),
            .dynamic_bytes = 0
        };
    }

private:
    static constexpr size_t MASK = Capacity - 1;
    std::array<T, Capacity> buffer_{};
    size_t head_{0};
    size_t tail_{0};

#ifndef NDEBUG
    bool producer_slot_acquired_ = false;
    bool consumer_slot_acquired_ = false;
#endif
};


} // namespace local
} // namespace lcr

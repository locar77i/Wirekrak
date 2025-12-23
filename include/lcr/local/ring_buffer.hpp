#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <type_traits>


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
//   ring_buffer<WalFile, 1024> wal_ring;
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
class alignas(64) ring_buffer {
    static_assert((Capacity >= 2) && ((Capacity & (Capacity - 1)) == 0),
                  "Capacity must be power of two and >= 2");

public:
    ring_buffer() noexcept = default;
    ~ring_buffer() noexcept = default;

    // Non-copyable / non-movable
    ring_buffer(const ring_buffer&) = delete;
    ring_buffer& operator=(const ring_buffer&) = delete;

    // Push (copy)
    inline bool push(const T& item) noexcept {
        const size_t next = (head_ + 1) & MASK;
        if (next == tail_) [[unlikely]]
            return false; // full
        buffer_[head_] = item;
        head_ = next;
        return true;
    }

    // Push (move)
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
    inline bool emplace_push(Args&&... args) noexcept {
        const size_t next = (head_ + 1) & MASK;
        if (next == tail_) [[unlikely]]
            return false;
        buffer_[head_] = T(std::forward<Args>(args)...);
        head_ = next;
        return true;
    }

    // Pop
    inline bool pop(T& out) noexcept {
        if (tail_ == head_) [[unlikely]]
            return false; // empty
        out = std::move(buffer_[tail_]);
        tail_ = (tail_ + 1) & MASK;
        return true;
    }

    inline bool empty() const noexcept { return head_ == tail_; }

    inline bool full() const noexcept {
        return ((head_ + 1) & MASK) == tail_;
    }

    inline constexpr size_t capacity() const noexcept { return Capacity; }

    inline size_t size() const noexcept {
        return (head_ >= tail_) ? (head_ - tail_)
                                : (Capacity - (tail_ - head_));
    }

    inline void clear() noexcept {
        head_ = tail_ = 0;
    }

private:
    static constexpr size_t MASK = Capacity - 1;
    alignas(64) std::array<T, Capacity> buffer_{};
    size_t head_{0};
    size_t tail_{0};
};


} // namespace local
} // namespace lcr

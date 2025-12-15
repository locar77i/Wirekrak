#pragma once

#include <cstdint>


namespace lcr {


// Monotonic sequence number generator per instrument (single-threaded)
class alignas(64) sequence {
    uint64_t next_seq_;
    char pad_[64 - sizeof(uint64_t)];

public:
    explicit constexpr sequence(uint64_t start = 1) noexcept : next_seq_(start) {}
    // Disable copy semantics
    sequence(const sequence&) = delete;
    sequence& operator=(const sequence&) = delete;
    // Enable move semantics
    sequence(sequence&&) noexcept = default;
    sequence& operator=(sequence&&) noexcept = default;

    // Return next sequence number and increment
    inline uint64_t next() noexcept {
        return next_seq_++;
    }

    // Peek at the current sequence number without incrementing
    inline uint64_t current() const noexcept {
        return next_seq_;
    }

    // Optional: reset sequence number
    inline void reset(uint64_t start = 1) noexcept {
        next_seq_ = start;
    }
};
// Defensive static assertions
static_assert(sizeof(sequence) == 64, "sequence must be cache-line aligned");
static_assert(alignof(sequence) == 64, "sequence must be cache-line aligned");


} // namespace lcr

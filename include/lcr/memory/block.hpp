#pragma once

/*
================================================================================
block
================================================================================

Runtime-sized, reusable raw memory container.

Designed for:
  • Pool allocation
  • Transport message storage
  • Zero dynamic growth
  • Deterministic capacity
  • Explicit size tracking

Properties:
  • Heap-allocated once
  • Fixed capacity at construction
  • No reallocation
  • No ownership semantics (pool-managed)
  • Not thread-safe
  • O(1) operations

This is the minimal memory unit for buffer pools.
================================================================================
*/

#include <cstddef>
#include <cstdint>
#include <cassert>

#include "lcr/memory/footprint.hpp"


namespace lcr::memory {

class block {
public:
    explicit block(std::size_t capacity) noexcept
        : capacity_(capacity),
          data_(static_cast<char*>(::operator new(capacity)))
    {}

    ~block() noexcept {
        ::operator delete(data_);
    }

    block(const block&) = delete;
    block& operator=(const block&) = delete;

    block(block&&) = delete;
    block& operator=(block&&) = delete;

    // ---------------------------------------------------------------------
    // Raw access
    // ---------------------------------------------------------------------

    [[nodiscard]]
    inline char* data() noexcept {
        return data_;
    }

    [[nodiscard]]
    inline const char* data() const noexcept {
        return data_;
    }

    // ---------------------------------------------------------------------
    // Capacity
    // ---------------------------------------------------------------------

    [[nodiscard]]
    inline std::size_t capacity() const noexcept {
        return capacity_;
    }

    // ---------------------------------------------------------------------
    // Size tracking
    // ---------------------------------------------------------------------

    [[nodiscard]]
    inline std::size_t size() const noexcept {
        return size_;
    }

    inline void set_size(std::size_t s) noexcept {
#ifndef NDEBUG
        assert(s <= capacity_ && "memory block overflow");
#else
        if (s > capacity_) [[unlikely]] {
            __builtin_trap();
        }
#endif
        size_ = s;
    }

    inline std::size_t remaining() const noexcept {
        return capacity_ - size_;
    }

    inline void reset() noexcept {
        size_ = 0;
    }

    [[nodiscard]]
    inline footprint memory_usage() const noexcept {
        return footprint{
            .static_bytes = sizeof(block),
            .dynamic_bytes = capacity_
        };
    }

private:
    std::size_t capacity_;
    char* data_;
    std::size_t size_{0};
};

} // namespace lcr::memory

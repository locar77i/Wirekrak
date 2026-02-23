// ============================================================================
// raw_buffer
// ----------------------------------------------------------------------------
// Fixed-capacity, reusable, single-thread raw memory buffer.
//
// A lightweight utility for deterministic, allocation-free workflows.
// Provides explicit size tracking over a statically allocated byte array.
//
// Intended for use in performance-sensitive, single-threaded contexts
// where dynamic memory allocation is undesirable or prohibited.
//
// Typical use cases include:
//   • Serialization / encoding
//   • Formatting
//   • Temporary message assembly
//   • Binary or text protocol construction
//   • Stack-like scratch storage
//
// Properties:
//   • Compile-time fixed capacity
//   • No heap allocation
//   • No dynamic growth
//   • No implicit resizing
//   • No synchronization (NOT thread-safe)
//   • Explicit size management
//   • Deterministic memory footprint
//
// Guarantees:
//   • O(1) operations
//   • No exceptions
//   • No hidden behavior
//
// This type does not impose semantics on the stored data.
// It is a raw byte container with explicit lifetime and size control.
//
// Example:
//
//   lcr::local::raw_buffer<1024> buffer;
//
//   auto* ptr = buffer.data();
//   std::size_t written = encode(ptr);
//   buffer.set_size(written);
//
//   consume(buffer.data(), buffer.size());
//
// ============================================================================ 
#pragma once

#include <cstddef>
#include <cassert>
#include <string_view>

namespace lcr::local {

template <std::size_t Capacity>
class raw_buffer {
public:
    raw_buffer() noexcept = default;

    raw_buffer(const raw_buffer&) = delete;
    raw_buffer& operator=(const raw_buffer&) = delete;

    // ------------------------------------------------------------------------
    // Raw access
    // ------------------------------------------------------------------------

    [[nodiscard]] inline char* data() noexcept {
        return buffer_;
    }

    [[nodiscard]] inline const char* data() const noexcept {
        return buffer_;
    }

    // ------------------------------------------------------------------------
    // Capacity
    // ------------------------------------------------------------------------

    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

    // ------------------------------------------------------------------------
    // Size management
    // ------------------------------------------------------------------------

    [[nodiscard]] inline std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] inline std::size_t remaining() const noexcept {
        return Capacity - size_;
    }

    inline void set_size(std::size_t s) noexcept {
#ifndef NDEBUG
        assert(s <= Capacity && "raw_buffer overflow");
#else
        if (s > Capacity) [[unlikely]] {
            __builtin_trap(); // deterministic crash
        }
#endif
        size_ = s;
    }

    inline void reset() noexcept {
        size_ = 0;
    }

    inline void clear() noexcept {
        size_ = 0;
    }

    // ------------------------------------------------------------------------
    // Optional convenience view
    // ------------------------------------------------------------------------

    [[nodiscard]]
    inline std::string_view view() const noexcept {
        return std::string_view(buffer_, size_);
    }

private:
    alignas(64) char buffer_[Capacity];
    std::size_t size_ = 0;
};

} // namespace lcr::local

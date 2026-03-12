#pragma once

/*
===============================================================================
lcr::local::vector
===============================================================================

A fixed-capacity, stack-allocated vector designed for deterministic,
allocation-free environments.

This container provides a subset of the std::vector interface while
guaranteeing that no dynamic memory allocation occurs during its lifetime.

Elements are stored in a contiguous internal buffer whose capacity is known
at compile time. Storage is managed manually using placement-new and explicit
destruction, allowing precise control over object lifetimes.

-------------------------------------------------------------------------------
Design Goals
-------------------------------------------------------------------------------
- Zero heap allocations
- Deterministic memory footprint
- Cache-friendly contiguous storage
- Predictable performance characteristics
- Suitable for ultra-low-latency (ULL) systems

-------------------------------------------------------------------------------
Key Properties
-------------------------------------------------------------------------------
- Capacity is fixed at compile time via the template parameter `Capacity`
- Elements are constructed in-place using placement new
- Destructors are invoked manually when elements are removed or cleared
- No reallocation or capacity growth is ever performed
- Memory layout is contiguous and equivalent to a static array

-------------------------------------------------------------------------------
Memory Layout
-------------------------------------------------------------------------------

    [ size_ | element storage buffer ]

Where:
- size_ tracks the number of constructed elements
- the storage buffer reserves space for `Capacity` objects of type `T`

All elements reside directly inside the container object.

-------------------------------------------------------------------------------
Complexity Guarantees
-------------------------------------------------------------------------------

Operation        Complexity
--------------------------------
push_back        O(1)
emplace_back     O(1)
pop_back         O(1)
iteration        O(n)
clear            O(n)

No operation performs dynamic allocation.

-------------------------------------------------------------------------------
Safety Notes
-------------------------------------------------------------------------------
- Bounds checking is enforced with assertions in debug builds
- Behavior is undefined if capacity is exceeded
- Intended for controlled environments where capacity is known ahead of time

-------------------------------------------------------------------------------
Typical Use Cases
-------------------------------------------------------------------------------
- Protocol message construction
- Fixed-size batching buffers
- Small collections in hot paths
- Allocation-free request serialization
- Low-latency networking or trading systems

-------------------------------------------------------------------------------
Comparison with std::vector
-------------------------------------------------------------------------------

Feature                std::vector        lcr::local::vector
------------------------------------------------------------
Dynamic allocation     Yes                No
Capacity growth        Yes                No
Deterministic memory   No                 Yes
Real-time safe         No                 Yes
Compile-time capacity  No                 Yes

-------------------------------------------------------------------------------
Notes
-------------------------------------------------------------------------------
This container intentionally omits many features of std::vector
(reserve, insert, erase, allocator support) in order to maintain
a minimal and predictable implementation suitable for high-performance
systems.

===============================================================================
*/

#include <new>
#include <utility>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <type_traits>
#include <initializer_list>

#include "lcr/trap.hpp"


namespace lcr::local {

template<typename T, std::size_t Capacity>
class vector {

    static_assert(std::is_move_assignable_v<T>, "lcr::local::vector requires move-assignable elements for erase()");

public:

    using value_type     = T;
    using size_type      = std::size_t;
    using iterator       = T*;
    using const_iterator = const T*;

    static constexpr size_type max_size = Capacity;

    static constexpr size_type capacity() noexcept {
        return Capacity;
    }

public:

    constexpr vector() noexcept = default;

    constexpr vector(const vector& other) noexcept {
        for (const auto& v : other) {
            emplace_back(v);
        }
    }

    constexpr vector(vector&& other) noexcept {
        for (size_type i = 0; i < other.size_; ++i) {
            emplace_back(std::move(other.data()[i]));
        }
        other.clear();
    }

    constexpr vector(std::initializer_list<T> init) noexcept {
        LCR_ASSERT_MSG(init.size() <= Capacity, "lcr::local::vector<> - initializer_list exceeds vector capacity");
        for (const auto& v : init) {
            emplace_back(v);
        }
    }

    ~vector() {
        clear();
    }

    constexpr vector& operator=(const vector& other) noexcept {
        if (this == &other) {
            return *this;
        }

        clear();

        for (const auto& v : other) {
            emplace_back(v);
        }

        return *this;
    }

    constexpr vector& operator=(vector&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        clear();

        for (auto& v : other) {
            emplace_back(std::move(v));
        }

        other.clear();

        return *this;
    }

public:

    [[nodiscard]]
    constexpr size_type size() const noexcept {
        return size_;
    }

    [[nodiscard]]
    constexpr bool empty() const noexcept {
        return size_ == 0;
    }

    [[nodiscard]]
    constexpr bool full() const noexcept {
        return size_ == Capacity;
    }

public:
    [[nodiscard]]
    T& operator[](size_type i) noexcept {
        LCR_ASSERT_MSG(i < size_, "lcr::local::vector index out of bounds");
        return data()[i];
    }

    [[nodiscard]]
    const T& operator[](size_type i) const noexcept {
        LCR_ASSERT_MSG(i < size_, "lcr::local::vector index out of bounds");
        return data()[i];
    }

    [[nodiscard]]
    T& front() noexcept {
        LCR_ASSERT_MSG(size_ > 0, "lcr::local::vector is empty");
        return data()[0];
    }

    [[nodiscard]]
    const T& front() const noexcept {
        LCR_ASSERT_MSG(size_ > 0, "lcr::local::vector is empty");
        return data()[0];
    }

    [[nodiscard]]
    T& back() noexcept {
        LCR_ASSERT_MSG(size_ > 0, "lcr::local::vector is empty");
        return data()[size_-1];
    }

    [[nodiscard]]
    const T& back() const noexcept {
        LCR_ASSERT_MSG(size_ > 0, "lcr::local::vector is empty");
        return data()[size_-1];
    }

public:
    [[nodiscard]]
    constexpr iterator begin() noexcept {
        return data();
    }

    [[nodiscard]]
    constexpr iterator end()   noexcept {
        return data() + size_;
    }

    [[nodiscard]]
    constexpr const_iterator begin() const noexcept {
        return data();
    }

    [[nodiscard]]
    constexpr const_iterator end()   const noexcept {
        return data() + size_;
    }

public:

    [[nodiscard]]
    T* data() noexcept {
        return std::launder(reinterpret_cast<T*>(storage_));
    }

    [[nodiscard]]
    const T* data() const noexcept {
        return std::launder(reinterpret_cast<const T*>(storage_));
    }

public:

    template<class... Args>
    T& emplace_back(Args&&... args) noexcept {
        LCR_ASSERT_MSG(size_ < Capacity, "lcr::local::vector capacity exceeded");

        T* ptr = new (data() + size_) T(std::forward<Args>(args)...);
        ++size_;

        return *ptr;
    }

    void push_back(const T& v) noexcept {
        emplace_back(v);
    }

    void push_back(T&& v) noexcept {
        emplace_back(std::move(v));
    }

    void pop_back() noexcept {
        LCR_ASSERT_MSG(size_ > 0, "lcr::local::vector underflow");

        --size_;
        if constexpr (!std::is_trivially_destructible_v<T>) {
            data()[size_].~T();
        }
    }

public:

    iterator erase(iterator pos) noexcept {
        LCR_ASSERT_MSG(pos >= begin() && pos < end(), "lcr::local::vector erase position out of bounds");

        iterator last = end() - 1;

        if constexpr (std::is_trivially_move_assignable_v<T>) {
            const auto count = last - pos;
            if (count > 0) [[likely]] {
                std::memmove(pos, pos + 1, count * sizeof(T));
            }
        }
        else {
            for (iterator it = pos; it < last; ++it) {
                *it = std::move(*(it + 1));
            }
        }

        pop_back();

        return pos;
    }

    iterator erase(iterator first, iterator last) noexcept {
        LCR_ASSERT_MSG(first >= begin(), "lcr::local::vector erase range out of bounds");
        LCR_ASSERT_MSG(last  <= end(), "lcr::local::vector erase range out of bounds");
        LCR_ASSERT_MSG(first <= last, "lcr::local::vector erase range out of bounds");

        iterator dst = first;
        iterator src = last;

        while (src != end()) {
            *dst++ = std::move(*src++);
        }

        size_type removed = last - first;

        for (size_type i = 0; i < removed; ++i) {
            pop_back();
        }

        return first;
    }

    iterator swap_erase(iterator pos) noexcept {
        LCR_ASSERT_MSG(pos >= begin() && pos < end(), "lcr::local::vector swap_erase position out of bounds");

        iterator last = end() - 1;

        if (pos != last) [[likely]] {
            *pos = std::move(*last);
        }

        pop_back();

        return pos;
    }

public:

    void clear() noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            T* d = data();
            for (size_type i = size_; i > 0; --i) {
                d[i-1].~T();
            }
        }
        size_ = 0;
    }

private:
    size_type size_{0};
    std::aligned_storage_t<sizeof(T), alignof(T)> storage_[Capacity];
};

} // namespace lcr::local

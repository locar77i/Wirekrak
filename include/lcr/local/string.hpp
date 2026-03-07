#pragma once

/*
===============================================================================
lcr::local::string<N>
===============================================================================

A fixed-capacity, stack-allocated string designed for deterministic,
allocation-free environments.

This container provides a lightweight alternative to `std::string` while
guaranteeing that no dynamic memory allocation occurs during its lifetime.

Characters are stored in an internal buffer whose capacity is known at
compile time. The string behaves as a mutable view over this storage.

-------------------------------------------------------------------------------
Design Goals
-------------------------------------------------------------------------------
- Zero heap allocations
- Deterministic memory footprint
- Cache-friendly contiguous storage
- Ultra-low-latency (ULL) safe
- Minimal interface compatible with std::string_view

-------------------------------------------------------------------------------
Key Properties
-------------------------------------------------------------------------------
- Capacity is fixed at compile time via template parameter `Capacity`
- Characters are stored directly inside the object
- No reallocation or dynamic growth is possible
- Compatible with `std::string_view`
- Supports fast comparison via `memcmp`

-------------------------------------------------------------------------------
Memory Layout
-------------------------------------------------------------------------------

    [ size_ | char buffer ]

Where:
- size_ stores the current length of the string
- buffer stores up to `Capacity` characters

A null terminator is maintained internally to support c_str().

-------------------------------------------------------------------------------
Complexity Guarantees
-------------------------------------------------------------------------------

Operation        Complexity
--------------------------------
assign           O(n)
copy             O(n)
comparison       O(n)
size             O(1)
view             O(1)

No operation performs dynamic allocation.

-------------------------------------------------------------------------------
Safety Notes
-------------------------------------------------------------------------------
- Bounds are checked via assertions in debug builds
- Behavior is undefined if capacity is exceeded
- Intended for controlled environments with known limits

-------------------------------------------------------------------------------
Typical Use Cases
-------------------------------------------------------------------------------
- Exchange symbols (BTC/USD, ETH/USD, etc.)
- Channel names
- Protocol identifiers
- Allocation-free message parsing
- Deterministic networking / trading systems

-------------------------------------------------------------------------------
Comparison with std::string
-------------------------------------------------------------------------------

Feature                std::string       lcr::local::string
------------------------------------------------------------
Dynamic allocation     Yes               No
Capacity growth        Yes               No
Deterministic memory   No                Yes
Real-time safe         No                Yes
Compile-time capacity  No                Yes

===============================================================================
*/

#include <cstdint>
#include <cstring>
#include <cstddef>
#include <string_view>
#include <ostream>
#include <cassert>
#include <limits>


namespace lcr::local {

template<std::size_t Capacity>
class string {

    static_assert(Capacity <= std::numeric_limits<std::uint16_t>::max(), "lcr::local::string capacity must fit within std::uint16_t");

public:

    using value_type = char;
    using size_type  = std::size_t;

public:

    constexpr string() noexcept {
        data_[0] = '\0';
    }

    constexpr explicit string(std::string_view s) {
        assign(s);
    }

    constexpr string(const char* s)
        : string(std::string_view{s}) {}

public:

    // -------------------------------------------------------------------------
    // Capacity
    // -------------------------------------------------------------------------

    [[nodiscard]]
    static constexpr size_type capacity() noexcept {
        return Capacity;
    }

    [[nodiscard]]
    static constexpr size_type max_size() noexcept {
        return Capacity;
    }

    [[nodiscard]]
    constexpr size_type size() const noexcept {
        return size_;
    }

    [[nodiscard]]
    constexpr bool empty() const noexcept {
        return size_ == 0;
    }

public:

    // -------------------------------------------------------------------------
    // Data access
    // -------------------------------------------------------------------------

    [[nodiscard]]
    constexpr const char* data() const noexcept {
        return data_;
    }

    [[nodiscard]]
    constexpr char* data() noexcept {
        return data_;
    }

    [[nodiscard]]
    constexpr const char* c_str() const noexcept {
        return data_;
    }

    [[nodiscard]]
    constexpr std::string_view view() const noexcept {
        return {data_, size_};
    }

    constexpr operator std::string_view() const noexcept {
        return view();
    }

public:

    // -------------------------------------------------------------------------
    // Element access
    // -------------------------------------------------------------------------

    [[nodiscard]]
    constexpr char& operator[](size_type i) noexcept {
        assert(i < size_);
        return data_[i];
    }

    [[nodiscard]]
    constexpr const char& operator[](size_type i) const noexcept {
        assert(i < size_);
        return data_[i];
    }

    [[nodiscard]]
    constexpr char front() const {
        assert(!empty());
        return data_[0];
    }

    [[nodiscard]]
    constexpr char back() const {
        assert(!empty());
        return data_[size_-1];
    }
    
public:

    // -------------------------------------------------------------------------
    // Modifiers
    // -------------------------------------------------------------------------

    constexpr void clear() noexcept {
        size_ = 0;
        data_[0] = '\0';
    }

    constexpr void assign(std::string_view s) {
        assert(s.size() <= Capacity);

        size_ = static_cast<std::uint16_t>(s.size());

        if (size_ > 0) [[likely]] {
            std::memcpy(data_, s.data(), size_);
        }
        data_[size_] = '\0';
    }

    constexpr void append(std::string_view s) {
        assert(size_ + s.size() <= Capacity);

        if (!s.empty()) [[likely]] {
            std::memcpy(data_ + size_, s.data(), s.size());
            size_ += s.size();
        }
        data_[size_] = '\0';
    }

    constexpr void push_back(char c) {
        assert(size_ < Capacity);

        data_[size_] = c;
        ++size_;
        data_[size_] = '\0';
    }

    constexpr void pop_back() {
        assert(size_ > 0);
        --size_;
        data_[size_] = '\0';
    }

    constexpr void resize(size_type n) {
        assert(n <= Capacity);
        size_ = n;
        data_[size_] = '\0';
    }

public:

    // -------------------------------------------------------------------------
    // Comparisons
    // -------------------------------------------------------------------------

    friend bool operator==(const string& a, const string& b) noexcept {
        return a.size_ == b.size_ && std::memcmp(a.data_, b.data_, a.size_) == 0;
    }

    friend bool operator!=(const string& a, const string& b) noexcept {
        return !(a == b);
    }

    friend bool operator==(const string& a, std::string_view b) noexcept {
        return a.size_ == b.size() && std::memcmp(a.data_, b.data(), a.size_) == 0;
    }

    friend bool operator!=(const string& a, std::string_view b) noexcept {
        return !(a == b);
    }

    friend bool operator==(const string& a, const char* b) noexcept {
        return a == std::string_view{b};
    }

    friend bool operator!=(const string& a, const char* b) noexcept {
        return !(a == b);
    }

    friend bool operator==(const char* a, const string& b) noexcept {
        return std::string_view{a} == b.view();
    }

    friend bool operator!=(const char* a, const string& b) noexcept {
        return !(a == b);
    }

    // --------------------------------------------------------------------------
    // Iterators
    // --------------------------------------------------------------------------

    [[nodiscard]]
    constexpr const char* begin() const noexcept {
        return data_;
    }

    [[nodiscard]]
    constexpr char* begin() noexcept {
        return data_;
    }

    [[nodiscard]]
    constexpr const char* end() const noexcept {
        return data_ + size_;
    }

    [[nodiscard]]
    constexpr char* end() noexcept {
        return data_ + size_;
    }

    // --------------------------------------------------------------------------
    // Utility
    // --------------------------------------------------------------------------

    [[nodiscard]]
    inline std::string to_string() const {
        return std::string(data(), size());
    }

private:

    char data_[Capacity+1]{};
    std::uint16_t size_{0};
};

template<std::size_t N>
inline std::ostream& operator<<(std::ostream& os, const string<N>& s) {
    return os.write(s.data(), s.size());
}

} // namespace lcr::local



#include <functional>

namespace std {

template<std::size_t N>
struct hash<lcr::local::string<N>> {

    std::size_t operator()(const lcr::local::string<N>& s) const noexcept {

        // FNV-1a hash (fast and common in trading engines)
        std::size_t hash = 14695981039346656037ull;

        const char* data = s.data();
        for (std::size_t i = 0; i < s.size(); ++i) {
            hash ^= static_cast<unsigned char>(data[i]);
            hash *= 1099511628211ull;
        }

        return hash;
    }
};

} // namespace std

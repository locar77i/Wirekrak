#pragma once

#include <cstdint>
#include <memory>
#include <type_traits>

#include "lcr/format.hpp"


namespace lcr {
namespace memory {

// Memory footprint structure
struct footprint {
    std::uint64_t static_bytes{0};
    std::uint64_t dynamic_bytes{0};

    inline constexpr std::uint64_t total_bytes() const noexcept {
        return static_bytes + dynamic_bytes;
    }

    template<typename T>
    static constexpr bool has_memory_usage =
        requires(const T& t) { t.memory_usage(); };

/*
    inline constexpr void add(const footprint& other) noexcept {
        static_bytes += other.static_bytes;
        dynamic_bytes += other.dynamic_bytes;
    }

    template <typename T>
    inline constexpr void add(const T& component) noexcept {
        if constexpr (has_memory_usage<T>) {
            add(component.memory_usage());
        }
    }
*/
    template <typename T>
    inline constexpr void add(const std::unique_ptr<T>& ptr) noexcept {
        if constexpr (has_memory_usage<T>) {
            if (ptr) {
                add_dynamic(ptr->memory_usage().total_bytes());
            }
        }
    }

    inline constexpr void add_static(std::uint64_t bytes) noexcept {
        static_bytes += bytes;
    }

    // Generic helper for subcomponents that expose memory_usage()
    template <typename T>
    inline constexpr void add_static(const T& component) noexcept {
        if constexpr (has_memory_usage<T>) {
            static_bytes += component.memory_usage().static_bytes;
        }
    }

    inline constexpr void add_dynamic(std::uint64_t bytes) noexcept {
        dynamic_bytes += bytes;
    }

    // Generic helper for subcomponents that expose memory_usage()
    template <typename T>
    inline constexpr void add_dynamic(const T& component) noexcept {
        if constexpr (has_memory_usage<T>) {
            dynamic_bytes += component.memory_usage().dynamic_bytes;
        }
    }

    inline void assert_under_limit(std::uint64_t static_limit, std::uint64_t dynamic_limit) const {
        assert(static_bytes <= static_limit && "lcr::memory::footprint - static memory usage exceeded limit");
        assert(dynamic_bytes <= dynamic_limit && "lcr::memory::footprint - dynamic memory usage exceeded limit");
    }

    // debug dump method for easy logging
    inline void debug_dump(std::ostream& os) const {
        os << "\n=== Memory Footprint ===\n";
        os << "  Static:  " << format_bytes(static_bytes) << "\n";
        os << "  Dynamic: " << format_bytes(dynamic_bytes) << "\n";
        os << "  Total:   " << format_bytes(total_bytes()) << "\n";
    }
};


} // namespace memory
} // namespace lcr

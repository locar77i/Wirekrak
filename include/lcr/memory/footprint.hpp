#pragma once

#include <cstdint>
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

    /// Merge another footprint into this one
    inline constexpr void add(const footprint& other) noexcept {
        static_bytes += other.static_bytes;
        dynamic_bytes += other.dynamic_bytes;
    }

    template <typename T>
    inline constexpr void add(const T& component) noexcept {
        if constexpr (requires { component.memory_usage(); }) {
            add(component.memory_usage());
        } else {
            static_assert(sizeof(T) == 0, "Type passed to add_component() must have memory_usage()");
        }
    }

    inline constexpr void add_static(std::uint64_t bytes) noexcept {
        static_bytes += bytes;
    }

    // Generic helper for subcomponents that expose memory_usage()
    template <typename T>
    inline constexpr void add_static(const T& component) noexcept {
        if constexpr (requires { component.memory_usage(); }) {
            const auto sub = component.memory_usage();
            static_bytes += sub.static_bytes;
        } else {
            static_assert(sizeof(T) == 0, "Type passed to add_static() must implement memory_usage()");
        }
    }

    inline constexpr void add_dynamic(std::uint64_t bytes) noexcept {
        dynamic_bytes += bytes;
    }

    // Generic helper for subcomponents that expose memory_usage()
    template <typename T>
    inline constexpr void add_dynamic(const T& component) noexcept {
        if constexpr (requires { component.memory_usage(); }) {
            const auto sub = component.memory_usage();
            dynamic_bytes += sub.total_bytes();
        } else {
            static_assert(sizeof(T) == 0, "Type passed to add_dynamic() must implement memory_usage()");
        }
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

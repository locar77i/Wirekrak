// ============================================================================
// JSON Writable Concepts
// ----------------------------------------------------------------------------
//
// These concepts define the contract for allocation-free JSON serialization
// within Wirekrak Core.
//
// The goal is to guarantee:
//
//   • No heap allocation during serialization
//   • Deterministic latency
//   • Explicit and bounded memory usage
//   • No hidden std::string construction
//   • Zero exception guarantees
//
// Two categories of JSON-serializable types are supported:
//
// -----------------------------------------------------------------------------
// 1. StaticJsonWritable
// -----------------------------------------------------------------------------
//
// Represents schema types whose maximum serialized size is:
//
//   • Known at compile time
//   • Independent of runtime object state
//   • Fully constant-evaluated
//
// Requirements:
//   • static constexpr max_json_size() noexcept
//   • std::size_t write_json(char*) const noexcept
//
// These types are fully compile-time bounded and allow stack sizing via
// RequestT::max_json_size() in constant expressions.
//
// Typical examples:
//   • Ping
//   • Small control messages
//   • Fixed-format protocol frames
//
// -----------------------------------------------------------------------------
// 2. DynamicJsonWritable
// -----------------------------------------------------------------------------
//
// Represents schema types whose maximum serialized size:
//
//   • Depends on runtime data (e.g., vectors, strings)
//   • Is computed per instance
//   • Is still deterministic and bounded
//
// Requirements:
//   • std::size_t max_json_size() const noexcept
//   • std::size_t write_json(char*) const noexcept
//
// These types are runtime-bounded but still allocation-free.
//
// Typical examples:
//   • Subscribe requests with variable symbol lists
//   • Batch operations
//   • Messages containing dynamic string content
//
// -----------------------------------------------------------------------------
// 3. JsonWritable
// -----------------------------------------------------------------------------
//
// Unified concept that accepts both StaticJsonWritable and
// DynamicJsonWritable types.
//
// This allows Session and transport layers to operate generically on
// allocation-free JSON types without caring whether the maximum size
// is compile-time or runtime determined.
//
// Design Philosophy
// -----------------
//
// Wirekrak distinguishes between:
//
//   • Compile-time bounded protocol messages
//   • Runtime bounded but allocation-free protocol messages
//
// This separation allows strict control messages to remain fully static,
// while still supporting variable-length protocol constructs.
//
// All JsonWritable types must:
//
//   • Serialize directly into caller-provided buffers
//   • Never allocate
//   • Never throw
//   • Provide an explicit maximum size calculation
//
// These guarantees are critical for ultra-low-latency (ULL) systems.
//
// ============================================================================

#pragma once

#include <concepts>
#include <cstddef>

namespace wirekrak::core::protocol {


// Represents schema types whose maximum serialized size is known at compile time,
// independent of runtime object state and fully constant-evaluated
template<typename T>
concept StaticJsonWritable =
    
    // 1. Must provide write_json and max_json_size
    requires(const T& t, char* buffer) {
        // Compile-time maximum serialized size
        { T::max_json_size() } noexcept -> std::convertible_to<std::size_t>;

        // Allocation-free JSON writer
        { t.write_json(buffer) } noexcept -> std::same_as<std::size_t>;
    }
    &&
    // 2. Must be usable as constant expression
    requires {
        // Forces constant-evaluated context
        requires (T::max_json_size() > 0);
    };


// Represents schema types whose maximum serialized size depends on runtime data (e.g., vectors),
// is computed per instance and is still deterministic and bounded
template<typename T>
concept DynamicJsonWritable =

    // 1. Must NOT satisfy StaticJsonWritable (to prevent ambiguity)
    (!StaticJsonWritable<T>)
    &&
    // 2. Must provide write_json and max_json_size
    requires(const T& t, char* buffer) {
        // Runtime-computed maximum serialized size
        { t.max_json_size() } noexcept -> std::convertible_to<std::size_t>;

        // Allocation-free JSON writer
        { t.write_json(buffer) } noexcept -> std::same_as<std::size_t>;
    };


// Unified concept that accepts both StaticJsonWritable and DynamicJsonWritable types
template<typename T>
concept JsonWritable = StaticJsonWritable<T> || DynamicJsonWritable<T>;

} // namespace wirekrak::core::protocol

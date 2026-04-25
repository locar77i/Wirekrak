#pragma once

/*
===============================================================================
type_list - Compile-time type container
===============================================================================

A lightweight utility for representing and manipulating a list of types at
compile time.

This is a fundamental building block used across the protocol layer for:

  • MessageBus          → message-type indexed storage
  • SubscriptionModel   → domain-type indexed storage
  • ReplayDatabase      → domain-type indexed storage
  • Static dispatch     → mapping types to indices

Design goals:
  - Zero runtime overhead
  - Fully constexpr / compile-time evaluated
  - No dependencies
  - Simple and predictable behavior

This is NOT intended to be a full metaprogramming library.
Only minimal, high-performance utilities are provided.

===============================================================================
*/

#include <cstddef>
#include <type_traits>

namespace wirekrak::core::meta {

// ============================================================================
// type_list
// ============================================================================
//
// A simple container for a variadic list of types.
//
// Example:
//   using MyTypes = type_list<int, double, float>;
//
// ============================================================================
template<typename... Ts>
struct type_list {
    static constexpr std::size_t size = sizeof...(Ts);
};

// ============================================================================
// type_list_size
// ============================================================================
template<typename List>
struct type_list_size;

template<typename... Ts>
struct type_list_size<type_list<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};

template<typename List>
inline constexpr std::size_t type_list_size_v = type_list_size<List>::value;

// ============================================================================
// type_list_contains
// ============================================================================
template<typename T, typename List>
struct type_list_contains;

template<typename T>
struct type_list_contains<T, type_list<>> : std::false_type {};

template<typename T, typename Head, typename... Tail>
struct type_list_contains<T, type_list<Head, Tail...>>
    : std::conditional_t<
        std::is_same_v<T, Head>,
        std::true_type,
        type_list_contains<T, type_list<Tail...>>
      > {};

template<typename T, typename List>
inline constexpr bool type_list_contains_v = type_list_contains<T, List>::value;

// ============================================================================
// type_list_index
// ============================================================================
//
// Returns the index of a type within a type_list.
//
// Requirements:
//   • T must exist in List
//
// Example:
//   using L = type_list<int, double, float>;
//   static_assert(type_list_index_v<double, L> == 1);
//
// ============================================================================
template<typename T, typename List>
struct type_list_index;

template<typename T, typename... Ts>
struct type_list_index<T, type_list<T, Ts...>>
    : std::integral_constant<std::size_t, 0> {};

template<typename T, typename U, typename... Ts>
struct type_list_index<T, type_list<U, Ts...>>
    : std::integral_constant<std::size_t, 1 + type_list_index<T, type_list<Ts...>>::value> {};

template<typename T, typename List>
inline constexpr std::size_t type_list_index_v = type_list_index<T, List>::value;

// ============================================================================
// type_list_at
// ============================================================================
//
// Retrieves the type at a given index.
//
// Example:
//   using L = type_list<int, double, float>;
//   using T = type_list_at_t<1, L>; // double
//
// ============================================================================
template<std::size_t I, typename List>
struct type_list_at;

template<std::size_t I, typename Head, typename... Tail>
struct type_list_at<I, type_list<Head, Tail...>>
    : type_list_at<I - 1, type_list<Tail...>> {};

template<typename Head, typename... Tail>
struct type_list_at<0, type_list<Head, Tail...>> {
    using type = Head;
};

template<std::size_t I, typename List>
using type_list_at_t = typename type_list_at<I, List>::type;

// ============================================================================
// type_list_for_each (compile-time iteration)
// ============================================================================
//
// Invokes a callable for each type in the list.
//
// The callable must provide:
//
//   template<class T>
//   void operator()();
//
// Example:
//
//   struct Visitor {
//       template<class T>
//       void operator()() {
//           // do something with T
//       }
//   };
//
//   type_list_for_each<type_list<int, float>>::apply(Visitor{});
//
// ============================================================================
template<typename List>
struct type_list_for_each;

template<typename... Ts>
struct type_list_for_each<type_list<Ts...>> {
    template<typename F>
    static constexpr void apply(F&& f) noexcept {
        (f.template operator()<Ts>(), ...);
    }
};

// ============================================================================
// type_list_assert_unique
// ============================================================================
//
// Ensures that all types in the list are unique.
//
// This is important for:
//   • Type → index mapping correctness
//   • Avoiding ambiguous dispatch
//
// ============================================================================
template<typename List>
struct type_list_assert_unique;

template<>
struct type_list_assert_unique<type_list<>> : std::true_type {};

template<typename Head, typename... Tail>
struct type_list_assert_unique<type_list<Head, Tail...>>
    : std::conditional_t<
        type_list_contains_v<Head, type_list<Tail...>>,
        std::false_type,
        type_list_assert_unique<type_list<Tail...>>
      > {};

template<typename List>
inline constexpr bool type_list_assert_unique_v = type_list_assert_unique<List>::value;

// ============================================================================
// Helper macro (optional)
// ============================================================================
//
// Static assert helper for cleaner diagnostics
//
// ============================================================================
#define WK_TYPE_LIST_ENSURE_UNIQUE(List) \
    static_assert(::wirekrak::core::meta::type_list_assert_unique_v<List>, \
        "type_list must not contain duplicate types")

} // namespace wirekrak::core::meta

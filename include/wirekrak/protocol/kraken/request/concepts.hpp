#pragma once

#include <type_traits>

namespace wirekrak::protocol::kraken::request {

/*
===============================================================================
Request Concepts (Compile-time API Safety)
===============================================================================

These concepts constrain the client API so that only valid request types
can be passed at compile time.

Each request type must explicitly encode its intent by defining exactly one
of the following tags:
  - subscribe_tag
  - unsubscribe_tag
  - control_tag

Design goals:
  - Zero runtime overhead
  - No inheritance or RTTI
  - Explicit intent encoded in the type system
  - Prevent subscribe/unsubscribe/control misuse at compile time
===============================================================================
*/

namespace detail {

// Count how many intent tags a type defines
template <typename T>
constexpr int intent_tag_count =
    (requires { typename T::subscribe_tag; }   ? 1 : 0) +
    (requires { typename T::unsubscribe_tag; } ? 1 : 0) +
    (requires { typename T::control_tag; }     ? 1 : 0);

} // namespace detail


// -----------------------------------------------------------------------------
// Subscription request
// -----------------------------------------------------------------------------
template <typename T>
concept Subscription =
    requires {
        typename T::subscribe_tag;
    };

// -----------------------------------------------------------------------------
// Unsubscription request
// -----------------------------------------------------------------------------
template <typename T>
concept Unsubscription =
    requires {
        typename T::unsubscribe_tag;
    };

// -----------------------------------------------------------------------------
// Control-plane request (e.g. system::Ping)
// -----------------------------------------------------------------------------
template <typename T>
concept Control =
    requires {
        typename T::control_tag;
    };


// -----------------------------------------------------------------------------
// Validation helper (used for static_asserts in client API)
// -----------------------------------------------------------------------------
template <typename T>
concept ValidRequestIntent = detail::intent_tag_count<T> == 1;

} // namespace wirekrak::protocol::kraken::request

#pragma once

#include <type_traits>

namespace wirekrak::protocol::kraken::request {

/*
===============================================================================
Request Concepts (Compile-time API Safety)
===============================================================================

These concepts constrain client APIs so that only valid request types
can be passed at compile time.

Design goals:
  - Zero runtime overhead
  - No inheritance or RTTI
  - Explicit intent encoded in the type system
  - Prevent subscribe/unsubscribe misuse at compile time

Subscribe and Unsubscribe are modeled as distinct request types.
===============================================================================
*/

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

} // namespace wirekrak::protocol::kraken::request

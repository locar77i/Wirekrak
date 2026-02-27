#pragma once

#include <concepts>

#include "wirekrak/core/policy/transport/liveness.hpp"

namespace wirekrak::core::policy::transport {

/*
===============================================================================
 Connection Policy Bundle
===============================================================================

Single injection point for transport-level connection behavior.

The Connection owns:

- Logical connection lifecycle
- Retry / reconnection strategy
- Liveness monitoring
- Observable connection signals

This bundle prevents template parameter explosion by grouping all
connection-level policies into a single type.

-------------------------------------------------------------------------------
 Responsibilities
-------------------------------------------------------------------------------

The bundle currently forwards:

  - liveness policy

Future extensions may include:

  - retry/backoff policy
  - reconnection limits
  - jitter strategy
  - idle policy
  - signal overflow behavior

-------------------------------------------------------------------------------
 Design Principles
-------------------------------------------------------------------------------

• Fully compile-time configuration
• Zero runtime polymorphism
• No virtual dispatch
• Deterministic per Connection type
• Transport-layer only (no protocol semantics)

-------------------------------------------------------------------------------
 Example
-------------------------------------------------------------------------------

using MyConnectionPolicies = connection_bundle<
    Active<std::chrono::seconds(15), 0.8>
>;

using MyConnection =
    Connection<MyWebSocket, MyMessageRing, MyConnectionPolicies>;

===============================================================================
*/


// ============================================================================
// Connection Policy Bundle
// ============================================================================

template<
    LivenessPolicy LivenessT = liveness::Enabled<>
>
struct connection_bundle {

    using liveness = LivenessT;

    // Future connection-level policies go here
};


// ============================================================================
// Default Bundle
// ============================================================================

using ConnectionDefault = connection_bundle<>;

} // namespace wirekrak::core::policy::transport
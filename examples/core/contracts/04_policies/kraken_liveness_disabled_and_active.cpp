// ============================================================================
// Core Contracts Example - Transport Liveness Disabled
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Transport Liveness  : Disabled
// Protocol Liveness   : Active (or Passive - behavior identical)
//
// EXPECTED BEHAVIOR
// -----------------
// - After connection, no subscriptions are issued.
// - No protocol traffic occurs.
// - Transport does NOT monitor inactivity.
// - No LivenessThreatened signal.
// - No LivenessExpired event.
// - No reconnect.
// - Connection remains idle indefinitely.
//
// This demonstrates that transport defines whether inactivity is
// considered a failure condition.
//
// ============================================================================

#include "common/run_zero_subscriptions_example.hpp"

#include "wirekrak/core/preset/transport/websocket_default.hpp"
#include "wirekrak/core/preset/control_ring_default.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"

using namespace wirekrak::core;

// -----------------------------------------------------------------------------
// Session Setup
// -----------------------------------------------------------------------------

// Transport: Liveness Disabled
using MyConnectionPolicies =
    policy::transport::connection_bundle<
        policy::transport::liveness::Disabled
    >;

// Protocol: Active (demonstrates that it does nothing without transport signal)
using MySessionPolicies =
    policy::protocol::session_bundle<
        policy::protocol::backpressure::Strict<>,
        policy::protocol::liveness::Active
    >;

using MySession =
    protocol::kraken::Session<
        preset::transport::DefaultWebSocket,
        preset::DefaultMessageRing,
        MySessionPolicies,
        MyConnectionPolicies
    >;


// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main(int argc, char** argv) {

    return run_zero_subscriptions_example<MySession, preset::DefaultMessageRing>(argc, argv,
        "Wirekrak Core - Transport Liveness Disabled Example\n"
        "Inactivity is not considered a failure.\n"
    );
}
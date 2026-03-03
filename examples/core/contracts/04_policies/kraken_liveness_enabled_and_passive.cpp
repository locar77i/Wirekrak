// ============================================================================
// Core Contracts Example - Passive Liveness Policy
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Transport Liveness  : Enabled
// Protocol Liveness   : Passive
//
// EXPECTED BEHAVIOR
// -----------------
// - After connection, no subscriptions are issued.
// - No protocol traffic occurs.
// - Transport liveness window expires.
// - LivenessExpired is emitted.
// - Connection closes.
// - Reconnect cycle begins.
//
// This demonstrates transport-driven liveness enforcement.
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

using MyConnectionPolicies =
    policy::transport::connection_bundle<
        policy::transport::liveness::Enabled<10000, 80> // 10s timeout
    >;

using MySessionPolicies =
    policy::protocol::session_bundle<
        policy::protocol::backpressure::Strict<>,
        policy::protocol::liveness::Passive
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
        "Wirekrak Core - Passive Liveness Example\n"
        "Transport enforces liveness; Session does not intervene.\n"
    );
}
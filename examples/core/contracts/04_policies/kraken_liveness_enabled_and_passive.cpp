// ============================================================================
// Core Contracts Example - Passive Liveness Policy
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Transport Liveness : Enabled (10s timeout, 80% warning threshold)
// Protocol Liveness  : Passive
//
// Backpressure Policy: Strict
//
// EXPECTED BEHAVIOR
// -----------------
// - The example establishes a WebSocket connection.
// - No subscriptions are issued.
// - No protocol traffic is generated.
//
// Because the transport layer monitors activity:
//
//     * The liveness warning window is reached
//     * A LivenessThreatened signal is emitted
//     * The liveness timeout expires
//     * The Connection closes the transport
//     * The retry cycle begins
//
// OBSERVATION GUIDE
// -----------------
// After the connection is established:
//
// - Around 8 seconds of inactivity:
//       LivenessThreatened signal
//
// - Around 10 seconds of inactivity:
//       LivenessExpired event
//       transport disconnect
//       reconnect cycle
//
// DESIGN PURPOSE
// --------------
// This example demonstrates transport-driven liveness enforcement.
//
// The protocol-level liveness policy is Passive, meaning:
//
//     * The Session observes liveness signals
//     * It does not actively intervene
//     * Recovery is entirely handled by the transport layer
//
// This configuration is useful when:
//
// - Transport reliability mechanisms are trusted
// - The protocol layer should remain purely reactive
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
        policy::transport::liveness::Enabled<10000, 80> // 10s timeout, 80% warning threshold
    >;

using MySessionPolicies =
    policy::protocol::session_bundle<
        policy::protocol::DefaultBackpressure,
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
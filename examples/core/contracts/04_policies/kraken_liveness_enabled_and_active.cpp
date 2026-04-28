// ============================================================================
// Core Contracts Example - Active Liveness Policy
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Transport Liveness : Enabled (10s timeout, 80% warning threshold)
// Protocol Liveness  : Active
//
// Backpressure Policy: Strict
//
// EXPECTED BEHAVIOR
// -----------------
// - The example establishes a WebSocket connection.
// - No subscriptions are issued.
// - No protocol traffic is generated.
//
// Because the transport layer monitors inactivity:
//
//     * The liveness warning window is reached
//     * A LivenessThreatened signal is emitted
//
// With Active protocol liveness enabled:
//
//     * The Session proactively sends a ping()
//     * Transport activity is observed
//     * Liveness is restored
//
// The connection therefore remains active and no reconnect occurs.
//
// OBSERVATION GUIDE
// -----------------
// After the connection is established:
//
// - Around 8 seconds of inactivity:
//       LivenessThreatened signal
//       Session sends ping()
//
// - A transport response restores activity
//       Liveness timeout is avoided
//
// The connection continues running normally.
//
// DESIGN PURPOSE
// --------------
// This example demonstrates protocol-driven liveness recovery.
//
// When the protocol liveness policy is Active, the Session reacts to
// transport liveness warnings by generating activity (ping) before the
// timeout expires.
//
// This allows the protocol layer to maintain the connection without
// forcing a reconnect cycle.
//
// ============================================================================

#include "wirekrak/core/protocol/session.hpp"
#include "wirekrak/core/protocol/kraken_model.hpp"
#include "wirekrak/core/preset/transport/websocket_default.hpp"
#include "wirekrak/core/preset/control_ring_default.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"

#include "common/run_zero_subscriptions_example.hpp"
#include "common/default_memory_pool.hpp"


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
        policy::protocol::liveness::Active
    >;

using MySession =
    protocol::Session<
        protocol::KrakenModel,
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
        "Wirekrak Core - Active Liveness Example\n"
        "Session proactively maintains transport liveness.\n"
        , wirekrak::examples::default_memory_pool
    );
}
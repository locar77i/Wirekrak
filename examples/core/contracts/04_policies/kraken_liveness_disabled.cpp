// ============================================================================
// Core Contracts Example - Transport Liveness Disabled
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Transport Liveness : Disabled
// Protocol Liveness  : Active
//
// Backpressure Policy: Strict
//
// EXPECTED BEHAVIOR
// -----------------
// - The example establishes a WebSocket connection.
// - No subscriptions are issued.
// - No protocol traffic is generated.
// - The transport layer does NOT monitor inactivity.
//
// As a result:
//
//     * No LivenessThreatened signal is emitted
//     * No LivenessExpired event occurs
//     * No automatic reconnection is triggered
//
// The connection remains idle indefinitely until the application
// explicitly closes it.
//
// OBSERVATION GUIDE
// -----------------
// After the connection is established:
//
// - No messages will be received.
// - No liveness warnings appear.
// - The connection remains stable even with prolonged inactivity.
//
// DESIGN PURPOSE
// --------------
// This example demonstrates that inactivity detection is a
// responsibility of the transport layer.
//
// When transport liveness monitoring is disabled, the system
// treats inactivity as a valid steady state.
//
// The protocol-level liveness policy remains active but has no
// effect because it relies on transport-level signals.
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

// Transport: Liveness Disabled
using MyConnectionPolicies =
    policy::transport::connection_bundle<
        policy::transport::liveness::Disabled
    >;

// Protocol liveness enabled - demonstrates that it has no effect
// when transport liveness monitoring is disabled.
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
        MySessionPolicies
    >;

    
// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main(int argc, char** argv) {

    return run_zero_subscriptions_example<MySession, preset::DefaultMessageRing>(argc, argv,
        "Wirekrak Core - Transport Liveness Disabled Example\n"
        "Inactivity is not considered a failure.\n",
        wirekrak::examples::default_memory_pool
    );
}
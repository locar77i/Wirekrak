// ============================================================================
// Core Contracts Example - Protocol Replay Disabled
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Transport Liveness : Enabled (Default)
// Transport Retry    : Enabled (Default)
// Protocol Replay    : Disabled
//
// EXPECTED BEHAVIOR
// -----------------
// - The example establishes a WebSocket connection.
// - A single trade subscription is issued (BTC/EUR).
// - If the transport reconnects after a network interruption,
//   the session does NOT replay previously acknowledged subscriptions.
// - The connection is restored but the session starts empty.
//
// OBSERVATION GUIDE
// -----------------
// 1. Start the example normally.
// 2. Disable your network connection to trigger a disconnect.
// 3. Re-enable the network to allow transport reconnection.
// 4. Observe that the connection reconnects successfully.
// 5. No automatic subscription replay occurs.
//
// DESIGN PURPOSE
// --------------
// This example demonstrates the behavior of the protocol layer when the
// replay policy is disabled.
//
// In this configuration the Session does not automatically restore
// previously acknowledged subscriptions after reconnect.
//
// ============================================================================

#include "common/run_retry_example.hpp"

#include "wirekrak/core/preset/transport/websocket_default.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"

using namespace wirekrak::core;


// -----------------------------------------------------------------------------
// Session Setup
// -----------------------------------------------------------------------------

// Protocol policies (disable replay)
using MySessionPolicies =
    policy::protocol::session_bundle<
        policy::protocol::DefaultBackpressure,
        policy::protocol::DefaultLiveness,
        policy::protocol::DefaultSymbolLimit,
        policy::protocol::replay::Disabled
    >;

using MySession =
    protocol::kraken::Session<
        preset::transport::DefaultWebSocket,
        preset::DefaultMessageRing,
        MySessionPolicies
    >;


// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main(int argc, char** argv) {

    return run_retry_example<MySession, preset::DefaultMessageRing>(argc, argv,
        "Wirekrak Core - Protocol Replay Disabled Example\n"
        "Transport reconections do not trigger automatic replay.\n"
    );
}
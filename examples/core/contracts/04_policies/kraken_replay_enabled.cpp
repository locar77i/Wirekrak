// ============================================================================
// Core Contracts Example - Protocol Replay Enabled
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Transport Liveness : Enabled (Default)
// Transport Retry    : Enabled (Default)
// Protocol Replay    : Enabled
//
// EXPECTED BEHAVIOR
// -----------------
// - The example establishes a WebSocket connection.
// - A single trade subscription is issued (BTC/EUR).
// - If the transport connection is interrupted (e.g. by disabling the network),
//   the Connection automatically attempts to reconnect.
// - Once the connection is restored, the Session automatically replays
//   previously acknowledged subscriptions.
//
// OBSERVATION GUIDE
// -----------------
// 1. Start the example normally.
// 2. Disable your network connection to trigger a disconnect.
// 3. Observe the retry attempts issued by the Connection.
// 4. Re-enable the network to allow reconnection.
// 5. After reconnect, observe that the previous subscription is replayed
//    automatically by the Session.
//
// DESIGN PURPOSE
// --------------
// This example demonstrates the behavior of the protocol layer when the
// replay policy is enabled.
//
// In this configuration the Session automatically restores previously
// acknowledged subscriptions after reconnect, ensuring deterministic
// recovery of the protocol state.
//
// ============================================================================

#include "common/run_retry_example.hpp"

#include "wirekrak/core/preset/transport/websocket_default.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"

using namespace wirekrak::core;


// -----------------------------------------------------------------------------
// Session Setup
// -----------------------------------------------------------------------------

// Protocol policies (replay enabled)
using MySessionPolicies =
    policy::protocol::session_bundle<
        policy::protocol::backpressure::Strict<>,
        policy::protocol::liveness::Passive,
        policy::protocol::DefaultSymbolLimit,
        policy::protocol::replay::Enabled
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
        "Wirekrak Core - Protocol Replay Enabled Example\n"
        "Reconnect automatically restores previous subscriptions.\n"
    );
}

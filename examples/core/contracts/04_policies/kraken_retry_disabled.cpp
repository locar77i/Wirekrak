// ============================================================================
// Core Contracts Example - Transport Retry Disabled
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Transport Liveness : Enabled (Default)
// Transport Retry    : Disabled
//
// EXPECTED BEHAVIOR
// -----------------
// - The example establishes a WebSocket connection.
// - A single trade subscription is issued (BTC/EUR).
// - If the transport connection is interrupted (e.g. by disabling the network),
//   the Connection does NOT attempt to reconnect.
// - The session transitions directly to the Disconnected state.
//
// OBSERVATION GUIDE
// -----------------
// 1. Start the example normally.
// 2. Disable your network connection to trigger a transport failure.
// 3. Observe that the Connection emits a disconnect signal.
// 4. No retry attempts are scheduled.
// 5. The session remains disconnected.
//
// DESIGN PURPOSE
// --------------
// This example demonstrates the behavior of the transport layer when the
// retry policy is disabled.
//
// In this configuration the Connection does not attempt automatic recovery
// after transport failures. Recovery must be initiated explicitly by the
// application.
//
// ============================================================================

#include "wirekrak/core/protocol/session.hpp"
#include "wirekrak/core/protocol/kraken_model.hpp"
#include "wirekrak/core/preset/transport/websocket_default.hpp"
#include "wirekrak/core/preset/control_ring_default.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"

#include "common/run_retry_example.hpp"
#include "common/default_memory_pool.hpp"


// -----------------------------------------------------------------------------
// Session Setup
// -----------------------------------------------------------------------------

// Transport: Default liveness monitoring + retry disabled
using MyConnectionPolicies =
    policy::transport::connection_bundle<
        policy::transport::DefaultLiveness,
        policy::transport::retry::Disabled
    >;

using MySession =
    protocol::Session<
        protocol::KrakenModel,
        preset::transport::DefaultWebSocket,
        preset::DefaultMessageRing,
        policy::protocol::DefaultSession,
        MyConnectionPolicies
    >;


// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main(int argc, char** argv) {

    return run_retry_example<MySession, preset::DefaultMessageRing>(argc, argv,
        "Wirekrak Core - Transport Retry Disabled Example\n"
        "Transport failures do not trigger automatic reconnection.\n",
        wirekrak::examples::default_memory_pool
    );
}
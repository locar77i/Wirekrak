// ============================================================================
// Core Contracts Example - Transport Retry (Exponential)
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Transport Liveness : Enabled (Default)
// Transport Retry    : Exponential
//
// EXPECTED BEHAVIOR
// -----------------
// - The example establishes a WebSocket connection.
// - A single trade subscription is issued (BTC/EUR).
// - If the transport connection is interrupted (e.g. by disabling the network),
//   the Connection enters the retry cycle.
// - Reconnection attempts follow an exponential backoff policy.
//
// OBSERVATION GUIDE
// -----------------
// 1. Start the example normally.
// 2. Disable your network connection to trigger a transport failure.
// 3. Observe the retry attempts scheduled by the Connection.
// 4. Re-enable the network connection.
// 5. The session reconnects and resumes normal operation.
//
// DESIGN PURPOSE
// --------------
// This example demonstrates the transport retry policy and how the
// Connection automatically schedules reconnection attempts after
// transient transport failures.
//
// The retry strategy and backoff parameters are fully defined by the
// injected transport policy.
//
// ============================================================================

#include "common/run_retry_example.hpp"

#include "wirekrak/core/preset/transport/websocket_default.hpp"
#include "wirekrak/core/preset/control_ring_default.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"

using namespace wirekrak::core;

// -----------------------------------------------------------------------------
// Session Setup
// -----------------------------------------------------------------------------

// Transport: Default liveness monitoring + exponential retry
using MyConnectionPolicies =
    policy::transport::connection_bundle<
        policy::transport::DefaultLiveness,
        policy::transport::retry::Exponential<>
    >;

using MySession =
    protocol::kraken::Session<
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
        "Wirekrak Core - Transport Retry Example (Exponential)\n"
        "Transport failures trigger automatic reconnection.\n"
    );
}
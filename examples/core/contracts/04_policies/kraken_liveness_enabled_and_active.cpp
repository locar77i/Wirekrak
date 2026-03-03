// ============================================================================
// Core Contracts Example - Active Liveness Policy
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Transport Liveness  : Enabled
// Protocol Liveness   : Active
//
// EXPECTED BEHAVIOR
// -----------------
// - After connection, no subscriptions are issued.
// - No protocol traffic occurs.
// - Transport emits LivenessThreatened.
// - Session proactively sends ping().
// - Traffic observed.
// - Liveness restored.
// - Connection remains active.
//
// This demonstrates protocol-driven liveness recovery.
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
        "Wirekrak Core - Active Liveness Example\n"
        "Session proactively maintains transport liveness.\n"
    );
}
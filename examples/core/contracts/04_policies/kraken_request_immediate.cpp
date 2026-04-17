// ============================================================================
// Core Contracts Example - Immediate Request Policy
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Request Batching Policy : Immediate
//
// EXPECTED BEHAVIOR
// -----------------
// - Subscription requests are sent exactly as issued by the user.
// - No automatic splitting or batching of symbol lists occurs.
//
// Example:
//
//       subscribe(symbols = 25)
//
// results in:
//
//       subscribe(symbols = 25)
//
// The request is transmitted as a single protocol message regardless
// of symbol count.
//
// OBSERVATION GUIDE
// -----------------
// During execution you should observe:
//
// - Exactly one "Sending subscribe message" log per subscription call
// - The message contains the full symbol list
// - No intermediate batching logs appear
//
// DESIGN PURPOSE
// --------------
// This example demonstrates the default behavior of the protocol session.
//
// The Immediate policy:
//
//   * preserves the user's request exactly
//   * introduces no additional scheduling or batching logic
//   * minimizes latency between API call and network transmission
//
// This policy is appropriate when:
//
//   - Requests are already within exchange limits
//   - Maximum immediacy is desired
//   - The client application performs its own batching logic
//
// ============================================================================

#include "wirekrak/core/preset/transport/websocket_default.hpp"
#include "wirekrak/core/preset/control_ring_default.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"

#include "common/run_multi_subscription_example.hpp"
#include "common/default_memory_pool.hpp"


// -------------------------------------------------------------------------
// Session setup
// -------------------------------------------------------------------------

using MySessionPolicies =
    policy::protocol::session_bundle<
        policy::protocol::DefaultBackpressure,
        policy::protocol::DefaultLiveness,
        policy::protocol::DefaultProgress,
        policy::protocol::DefaultSymbolLimit,
        policy::protocol::DefaultReplay,
        policy::protocol::BatchingPolicy<
            policy::protocol::BatchingMode::Immediate    // Immediate mode (default batch size: 0 - default pacing interval: 0)
        >
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
    return run_multi_subscription_example<MySession, preset::DefaultMessageRing>(
        argc,
        argv,
        "Wirekrak Core - Immediate Request Policy Example\n"
        "Demonstrates default request behavior without batching.\n",
        wirekrak::examples::default_memory_pool
    );
}

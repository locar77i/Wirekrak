// ============================================================================
// Core Contracts Example - ZeroTolerance Backpressure Policy
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Transport Backpressure : ZeroTolerance
// Protocol Backpressure  : ZeroTolerance
//
// EXPECTED BEHAVIOR
// -----------------
// - The example issues multiple subscriptions to generate high message traffic.
// - If the transport message ring becomes saturated:
//
//     * The overload condition is detected immediately
//     * The connection is force-closed
//     * No recovery window is provided
//
// - The connection does NOT attempt to stabilize or clear backpressure.
//
// As a result:
//
//     * BackpressureDetected may be emitted
//     * The transport is closed immediately afterwards
//     * No BackpressureCleared event is ever emitted
//
// OBSERVATION GUIDE
// -----------------
// During sustained high throughput:
//
// - Once the message ring capacity is exceeded:
//       immediate transport shutdown
//
// - The connection transitions to the disconnect / retry cycle
//   depending on the configured retry policy.
//
// DESIGN PURPOSE
// --------------
// This example demonstrates the strictest backpressure safety model.
//
// The ZeroTolerance policy assumes that transport saturation represents
// a violation of system capacity assumptions.
//
// Instead of attempting recovery, the system immediately terminates the
// connection to preserve deterministic behavior and prevent uncontrolled
// overload propagation.
//
// This configuration is useful for:
//
// - Ultra-low-latency trading systems
// - Deterministic environments with strict capacity guarantees
// - Deployments prioritizing correctness over availability
//
// ============================================================================

#include "wirekrak/core/preset/protocol/kraken_default.hpp"


#include "common/run_multi_subscription_example.hpp"
#include "common/default_memory_pool.hpp"


// -------------------------------------------------------------------------
// Session setup
// -------------------------------------------------------------------------
using MyWebSocketPolicies =
    policy::transport::websocket_bundle<
        policy::transport::backpressure::ZeroTolerance
    >;

using MySessionPolicies = 
    policy::protocol::session_bundle<
        policy::protocol::backpressure::ZeroTolerance
    >;

using MyWebSocket =
    transport::WebSocket<
        preset::DefaultControlRing,
        preset::DefaultMessageRing,
        MyWebSocketPolicies
    >;

using MySession = 
    protocol::kraken::Session<
        MyWebSocket,
        preset::DefaultMessageRing,
        MySessionPolicies
    >;


// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    return run_multi_subscription_example<MySession, preset::DefaultMessageRing>(argc, argv,
        "Wirekrak Core - ZeroTolerance Backpressure Policy Example\n"
        "Immediate connection shutdown on overload.\n",
        wirekrak::examples::default_memory_pool
    );
}

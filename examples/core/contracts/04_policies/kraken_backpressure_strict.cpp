// ============================================================================
// Core Contracts Example - Strict Backpressure Policy
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Transport Backpressure : Strict
//     activation_threshold   = 1 overload event
//     deactivation_threshold = 8 recovery signals
//
// Protocol Backpressure : Strict
//     escalation_threshold = 24 consecutive overload signals
//
// EXPECTED BEHAVIOR
// -----------------
// - The example issues multiple subscriptions to generate message traffic.
// - High throughput may saturate the transport message ring.
//
// With the Strict transport policy:
//
//     * BackpressureDetected is emitted immediately on the first overload
//     * The overload condition remains active until recovery stabilizes
//     * BackpressureCleared is emitted only after consecutive recoveries
//
// With the Strict protocol policy:
//
//     * The Session observes transport backpressure signals
//     * If overload persists long enough, the Session escalates the condition
//
// This ensures that overload is surfaced deterministically while still
// preventing oscillation during recovery.
//
// OBSERVATION GUIDE
// -----------------
// During high message throughput:
//
// - First overload event:
//       BackpressureDetected signal
//
// - After the system recovers and the message ring drains:
//
//       BackpressureCleared signal
//
// If overload persists beyond the escalation threshold, the Session
// performs protocol-level mitigation.
//
// DESIGN PURPOSE
// --------------
// This example demonstrates deterministic overload detection with
// stabilization semantics.
//
// The Strict policy provides immediate visibility of resource saturation
// while still allowing recovery to settle before clearing the condition.
//
// This configuration is useful for:
//
// - Ultra-low-latency systems
// - Deterministic overload monitoring
// - Systems where the protocol layer controls mitigation strategy
//
// ============================================================================

#include "wirekrak/core/transport/websocket/engine.hpp"
#include "wirekrak/core/transport/websocket_concept.hpp"
#include "wirekrak/core/preset/transport/backend_default.hpp"
#include "wirekrak/core/preset/protocol/kraken_default.hpp"

#include "common/run_multi_subscription_example.hpp"
#include "common/default_memory_pool.hpp"


// -------------------------------------------------------------------------
// Session setup
// -------------------------------------------------------------------------

// Session escalates if overload persists for twice the activation threshold
constexpr std::size_t ESCALATION_THRESHOLD = 16;

using MyWebSocketPolicies =
    policy::transport::websocket_bundle<
        policy::transport::backpressure::Strict
    >;

using MySessionPolicies =
    policy::protocol::session_bundle<
        policy::protocol::backpressure::Strict
    >;

using MyWebSocket =
    transport::websocket::Engine<
        preset::DefaultControlRing,
        preset::DefaultMessageRing,
        MyWebSocketPolicies,
        preset::transport::DefaultBackend
    >;
// Assert that MyWebSocket conforms to transport::WebSocketConcept concept
static_assert(transport::WebSocketConcept<MyWebSocket>);

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
        "Wirekrak Core - Strict Backpressure Policy Example\n"
        "Demonstrates deterministic overload detection.\n",
        wirekrak::examples::default_memory_pool
    );
}

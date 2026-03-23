// ============================================================================
// Core Contracts Example - Relaxed Backpressure Policy
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Transport Backpressure : Relaxed
//     activation_threshold   = 64 overload signals
//     deactivation_threshold = 8 recovery signals
//
// Protocol Backpressure : Relaxed
//     escalation_threshold = 192 consecutive overload signals
//
// EXPECTED BEHAVIOR
// -----------------
// - The example issues multiple subscriptions to generate message traffic.
// - High throughput may temporarily overload the transport pipeline.
//
// With the Relaxed transport policy:
//
//     * Short overload bursts are tolerated
//     * No immediate BackpressureDetected signal is emitted
//     * Activation occurs only after sustained overload
//
// With the Relaxed protocol policy:
//
//     * The Session observes transport backpressure signals
//     * Escalation occurs only after prolonged overload persistence
//
// Temporary bursts therefore do NOT immediately trigger protocol-level
// mitigation.
//
// OBSERVATION GUIDE
// -----------------
// During high message throughput:
//
// - Short bursts of overload may occur without signaling
// - Sustained overload triggers BackpressureDetected
// - Recovery must stabilize before BackpressureCleared is emitted
//
// If overload persists long enough, the Session escalates the condition
// according to the protocol backpressure policy.
//
// DESIGN PURPOSE
// --------------
// This example demonstrates burst-tolerant overload handling.
//
// The Relaxed policy introduces hysteresis to avoid oscillation and
// control-plane noise in high-throughput environments.
//
// This configuration is useful for:
//
// - High-volume market data ingestion
// - Exchanges with bursty update patterns
// - Systems prioritizing availability over immediate throttling
//
// ============================================================================

#include "wirekrak/core/preset/protocol/kraken_default.hpp"

#include "common/run_multi_subscription_example.hpp"
#include "common/default_memory_pool.hpp"


// -------------------------------------------------------------------------
// Session setup
// -------------------------------------------------------------------------

// Session escalates if overload persists for twice the activation threshold
constexpr std::size_t ESCALATION_THRESHOLD = 264;

using MyWebSocketPolicies =
    policy::transport::websocket_bundle<
        policy::transport::backpressure::Relaxed
    >;

using MySessionPolicies =
    policy::protocol::session_bundle<
        policy::protocol::backpressure::Relaxed
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
        "Wirekrak Core - Relaxed Backpressure Policy Example\n"
        "Demonstrates burst-tolerant overload handling.\n",
        wirekrak::examples::default_memory_pool
    );
}

// ============================================================================
// Core Contracts Example — Relaxed Backpressure Policy
// ============================================================================
//
// POLICY BEHAVIOR
// ---------------
// Relaxed policy tolerates temporary overload before signaling.
//
//   - Activation threshold: N consecutive overloads
//   - Deactivation threshold: M consecutive recoveries
//   - Escalation handled by the session after persistent overload
//
// DESIGN PHILOSOPHY
// -----------------
// Relaxed policy assumes that short bursts are normal under market
// volatility and should not immediately trigger control-plane signals.
//
// This policy reduces oscillation and signal noise in burst-heavy
// environments.
//
// USE CASE
// --------
// - High-throughput market data ingestion
// - Environments with natural burst patterns
// - Systems optimizing for availability over strict immediacy
//
// EXPECTED BEHAVIOR
// -----------------
// - No immediate BackpressureDetected signal.
// - Activation only after sustained overload.
// - Stabilized recovery before clearing.
// - Session escalates only after prolonged persistence.
//
// This example demonstrates burst-tolerant overload handling.
//
// ============================================================================
#include "commom/run_backpressure_example.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"


// -------------------------------------------------------------------------
// Session setup
// -------------------------------------------------------------------------
// Number of consecutive signals before activation (for Relaxed policy)
constexpr std::size_t HYSTERESIS_ACTIVATION_THRESHOLD   = 64; 

// Number of consecutive signals before deactivation (for Relaxed policy)
constexpr std::size_t HYSTERESIS_DEACTIVATION_THRESHOLD = 8;

using MyWebSocketPolicies =
    policy::transport::websocket_bundle<
        policy::backpressure::Relaxed<
            HYSTERESIS_ACTIVATION_THRESHOLD,
            HYSTERESIS_DEACTIVATION_THRESHOLD
        >
    >;

using MySessionPolicies =
    policy::protocol::session_bundle<
        policy::backpressure::Relaxed<
            HYSTERESIS_ACTIVATION_THRESHOLD,
            HYSTERESIS_DEACTIVATION_THRESHOLD
        >
    >;

using MyWebSocket =
    transport::winhttp::WebSocketImpl<
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
    return run_backpressure_example<MySession, preset::DefaultMessageRing>(argc, argv,
        "Wirekrak Core — Protocol Backpressure Example (Relaxed)\n"
        "Demonstrates explicit backpressure handling with multiple subscriptions.\n",
        "This example runs indefinitely until interrupted.\n"
        "Press Ctrl+C to unsubscribe and exit cleanly.\n"
        "Let's enjoy trading with Wirekrak!"
    );
}

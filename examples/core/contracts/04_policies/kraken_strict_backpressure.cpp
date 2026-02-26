// ============================================================================
// Core Contracts Example — Strict Backpressure Policy
// ============================================================================
//
// POLICY BEHAVIOR
// ---------------
// Strict policy activates immediately on first overload detection,
// but allows stabilized recovery before clearing the condition.
//
//   - Activation threshold: 1 overload event
//   - Deactivation threshold: N consecutive recoveries
//   - Escalation handled by the session after persistent overload
//
// DESIGN PHILOSOPHY
// -----------------
// Strict policy enforces immediate visibility of backpressure while
// still allowing transient oscillations to settle before recovery.
//
// This prevents noise from short-lived ring saturation while still
// surfacing overload deterministically.
//
// USE CASE
// --------
// - Low-latency systems with bounded tolerance
// - Environments requiring deterministic overload visibility
// - Systems that prefer session-level decision control
//
// EXPECTED BEHAVIOR
// -----------------
// - BackpressureDetected emitted immediately.
// - BackpressureCleared emitted only after stabilized recovery.
// - Session escalates if overload persists across configured threshold.
//
// This example demonstrates deterministic overload handling with
// stabilization semantics.
//
// ============================================================================
#include "commom/run_backpressure_example.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"


// -------------------------------------------------------------------------
// Session setup
// -------------------------------------------------------------------------
// Number of consecutive signals before deactivation (for Strict policy)
constexpr std::size_t HYSTERESIS_DEACTIVATION_THRESHOLD = 8;

using MyWebSocketPolicies =
    policy::transport::websocket_bundle<
        policy::backpressure::Strict<HYSTERESIS_DEACTIVATION_THRESHOLD>
    >;

using MySessionPolicies =
    policy::protocol::session_bundle<
        policy::backpressure::Strict<HYSTERESIS_DEACTIVATION_THRESHOLD>
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
        "Wirekrak Core — Protocol Backpressure Example (Strict)\n"
        "Demonstrates explicit backpressure handling with multiple subscriptions.\n",
        "This example runs indefinitely until interrupted.\n"
        "Press Ctrl+C to unsubscribe and exit cleanly.\n"
        "Let's enjoy trading with Wirekrak!"
    );
}

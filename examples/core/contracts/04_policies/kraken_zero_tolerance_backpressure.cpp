// ============================================================================
// Core Contracts Example — ZeroTolerance Backpressure Policy
// ============================================================================
//
// POLICY BEHAVIOR
// ---------------
// ZeroTolerance represents the strictest correctness guarantee:
//
//   - On first transport saturation, the connection is force-closed.
//   - No hysteresis.
//   - No recovery window.
//   - No tolerance for overload.
//
// This policy assumes that transport backpressure indicates a violation
// of system capacity assumptions.
//
// DESIGN PHILOSOPHY
// -----------------
// ZeroTolerance prioritizes correctness over availability.
//
// If the protocol cannot keep up with the incoming message rate,
// the system is considered compromised and the connection is terminated
// immediately to preserve deterministic behavior.
//
// USE CASE
// --------
// - Ultra-low-latency trading systems
// - Environments where message loss or delay is unacceptable
// - Strict correctness-first deployments
//
// EXPECTED BEHAVIOR
// -----------------
// - Under sustained high load, connection closes immediately.
// - No BackpressureCleared event will ever be emitted.
// - Escalation is transport-driven.
//
// This example demonstrates the most conservative safety model.
//
// ============================================================================
#include "commom/run_backpressure_example.hpp"


// -------------------------------------------------------------------------
// Session setup
// -------------------------------------------------------------------------
using MyWebSocketPolicies =
    policy::transport::websocket_bundle<
        policy::backpressure::ZeroTolerance
    >;

using MySessionPolicies = 
    policy::protocol::session_bundle<
        policy::backpressure::ZeroTolerance
    >;

using MyMessageRing =
    lcr::lockfree::spsc_ring<
        transport::websocket::DataBlock,
        transport::RX_RING_CAPACITY
    >;

using MyWebSocket =
    transport::winhttp::WebSocketImpl<
        MyMessageRing,
        MyWebSocketPolicies
    >;

using MySession = 
    protocol::kraken::Session<
        MyWebSocket,
        MyMessageRing,
        MySessionPolicies
    >;

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    return run_backpressure_example<MySession, MyMessageRing>(argc, argv,
        "Wirekrak Core — Protocol Backpressure Example (ZeroTolerance)\n"
        "Demonstrates explicit backpressure handling with multiple subscriptions.\n",
        "This example runs indefinitely until interrupted.\n"
        "Press Ctrl+C to unsubscribe and exit cleanly.\n"
        "Let's enjoy trading with Wirekrak!"
    );
}

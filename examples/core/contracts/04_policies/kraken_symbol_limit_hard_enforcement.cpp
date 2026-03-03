// ============================================================================
// Core Contracts Example - Hard Symbol Limit Policy
// ============================================================================
//
// POLICY BEHAVIOR
// ---------------
// Hard SymbolLimit enforces compile-time subscription capacity constraints.
//
// - Subscriptions exceeding limits are rejected immediately.
// - No transport message is sent.
// - No partial allocation occurs.
// - No replay DB mutation occurs.
// - Deterministic behavior.
//
// DESIGN PHILOSOPHY
// -----------------
// Capacity assumptions are part of the system contract.
//
// The Session enforces subscription limits before interacting with the
// transport layer, preserving protocol correctness and preventing
// overload amplification.
//
// USE CASE
// --------
// - Exchange-imposed subscription caps
// - Risk-controlled deployments
// - Multi-tenant systems
//
// EXPECTED BEHAVIOR
// -----------------
// - Oversized subscription attempts are rejected synchronously.
// - req_id == INVALID_REQ_ID.
// - No network activity occurs.
//
// This example demonstrates compile-time capacity enforcement.
//
// ============================================================================

#include "common/run_multi_subscription_example.hpp"
#include "wirekrak/core/preset/transport/websocket_default.hpp"
#include "wirekrak/core/preset/control_ring_default.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"


// -------------------------------------------------------------------------
// Session setup
// -------------------------------------------------------------------------

using MySessionPolicies =
    policy::protocol::session_bundle<
        policy::protocol::backpressure::Strict<>,
        policy::protocol::liveness::Passive,
        policy::protocol::SymbolLimitPolicy<
            policy::protocol::LimitMode::Hard,
            2,  // max_trade
            1,  // max_book
            2   // max_global
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
    return run_multi_subscription_example<MySession, preset::DefaultMessageRing>(argc, argv,
        "Wirekrak Core - Protocol Symbol Limit Hard Enforcement Example\n"
        "Demonstrates hard symbol limit enforcement with multiple subscriptions.\n"
    );
}

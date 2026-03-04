// ============================================================================
// Core Contracts Example - Protocol Symbol Limit (Hard Enforcement)
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Protocol Symbol Limit : Hard
//
// Limits:
//   max_trade  = 2
//   max_book   = 1
//   max_global = 2
//
// Backpressure Policy   : Strict
// Protocol Liveness     : Passive
//
// EXPECTED BEHAVIOR
// -----------------
// - The example issues multiple subscription requests.
// - When a request exceeds the configured symbol limits:
//
//     * The request is rejected immediately.
//     * No transport message is sent.
//     * The replay database is not mutated.
//     * No partial allocation occurs.
//
// - The rejection is deterministic and synchronous.
//
// OBSERVATION GUIDE
// -----------------
// Observe the subscription requests printed by the example:
//
// - Valid requests within capacity are accepted.
// - Requests exceeding limits return:
//
//     req_id == INVALID_REQ_ID
//
// This indicates the request was rejected before reaching the
// transport layer.
//
// DESIGN PURPOSE
// --------------
// This example demonstrates deterministic capacity enforcement
// performed by the Session before interacting with the transport.
//
// Hard symbol limits are useful when:
//
// - Exchanges impose subscription caps
// - Systems require strict resource control
// - Multi-tenant environments must prevent overload
//
// By rejecting oversized requests early, the system preserves
// protocol correctness and prevents overload amplification.
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
        policy::protocol::DefaultBackpressure,
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
        "Wirekrak Core - Protocol Hard Symbol Limit Example\n"
        "Demonstrates deterministic subscription capacity enforcement.\n"
    );
}

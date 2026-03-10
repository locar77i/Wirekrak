// ============================================================================
// Core Contracts Example - Batch Request Policy
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Request Batching Policy : Batch
// Batch Size              : 10 symbols
// Batch Interval          : 0 ms (immediate)
//
// EXPECTED BEHAVIOR
// -----------------
// - The example issues large multi-symbol subscription requests.
// - If a request contains more symbols than the configured batch size:
//
//       symbols_per_request > batch_size
//
//   the request is automatically split into multiple protocol messages.
//
// Example:
//
//       subscribe(symbols = 25)
//
// becomes:
//
//       subscribe(symbols = 10)
//       subscribe(symbols = 10)
//       subscribe(symbols = 5)
//
// All generated requests:
//
//   * share the same req_id
//   * preserve the original subscription intent
//   * are transmitted sequentially.
//
// OBSERVATION GUIDE
// -----------------
// During execution you should observe:
//
// - Multiple "Sending subscribe message" logs for a single logical request
// - Each message containing at most 10 symbols
// - All messages sharing the same req_id
//
// The exchange will acknowledge each symbol independently while maintaining
// the logical grouping of the request.
//
// DESIGN PURPOSE
// --------------
// This example demonstrates deterministic request batching.
//
// Batching is useful when:
//
//   - Exchanges impose limits on symbols per request
//   - Very large subscription sets must be transmitted
//   - Network payload size should remain bounded
//
// The Batch policy:
//
//   * preserves deterministic request semantics
//   * avoids oversized protocol messages
//   * keeps request construction allocation-free
//
// Unlike paced batching, the Batch policy sends all batches immediately
// without introducing time-based throttling.
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
        policy::protocol::DefaultLiveness,
        policy::protocol::DefaultSymbolLimit,
        policy::protocol::DefaultReplay,
        policy::protocol::BatchingPolicy<
            policy::protocol::BatchingMode::Batch,   // Batch mode
            10       // batch size
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
        "Wirekrak Core - Batch Request Policy Example\n"
        "Demonstrates batching behavior with a batch size of 10 and zero interval.\n"
    );
}

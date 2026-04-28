// ============================================================================
// Core Contracts Example - Paced Request Batching Policy
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Request Batching Policy : Paced
// Batch Size              : 10 symbols
// Batch Interval          : 200 ms
//
// EXPECTED BEHAVIOR
// -----------------
// - Large subscription requests are split into smaller batches.
// - Each batch contains at most 10 symbols.
// - Batches are NOT sent immediately.
//
// Instead:
//
//       batch #1  → sent immediately
//       batch #2  → sent after 200 ms
//       batch #3  → sent after 200 ms
//       ...
//
// The request is therefore spread over time instead of creating
// a burst of messages.
//
// Example:
//
//       subscribe(symbols = 25)
//
// becomes:
//
//       subscribe(symbols = 10)  → immediately
//       subscribe(symbols = 10)  → after 200 ms
//       subscribe(symbols = 5)   → after 200 ms
//
// All generated requests:
//
//   * share the same req_id
//   * preserve the logical subscription intent
//   * are gradually transmitted during poll()
//
// OBSERVATION GUIDE
// -----------------
// During execution you should observe:
//
// - A short delay between subscribe messages
// - Each batch containing at most 10 symbols
// - Log timestamps showing pacing intervals
//
// Example logs:
//
//   Sending subscribe message: 10 symbol/s
//   (200ms delay)
//   Sending subscribe message: 10 symbol/s
//   (200ms delay)
//   Sending subscribe message: 5 symbol/s
//
// DESIGN PURPOSE
// --------------
// Paced batching is designed for environments where:
//
//   - Exchanges enforce strict request rate limits
//   - Massive subscription replays occur after reconnect
//   - Burst traffic should be avoided
//
// Instead of flooding the exchange with many requests at once,
// the Session gradually drains the batch queue during poll().
//
// This ensures:
//
//   * predictable request emission
//   * exchange-friendly behavior
//   * reduced reconnect storms
//
// ============================================================================

#include "wirekrak/core/protocol/session.hpp"
#include "wirekrak/core/protocol/kraken_model.hpp"
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
            policy::protocol::BatchingMode::Paced,
            10,      // batch size
            100      // emit interval
        >
    >;

using MySession =
    protocol::Session<
        protocol::KrakenModel,
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
        "Wirekrak Core - Paced Request Batching Example\n"
        "Demonstrates gradual request emission (10 symbols every 200ms).\n",
        wirekrak::examples::default_memory_pool
    );
}

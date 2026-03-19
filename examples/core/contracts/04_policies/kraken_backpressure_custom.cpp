// ============================================================================
// Core Contracts Example - Immediate Request Policy
// ============================================================================
//
// POLICY CONFIGURATION
// --------------------
// Request Batching Policy : Immediate
//
// EXPECTED BEHAVIOR
// -----------------
// - Subscription requests are sent exactly as issued by the user.
// - No automatic splitting or batching of symbol lists occurs.
//
// Example:
//
//       subscribe(symbols = 25)
//
// results in:
//
//       subscribe(symbols = 25)
//
// The request is transmitted as a single protocol message regardless
// of symbol count.
//
// OBSERVATION GUIDE
// -----------------
// During execution you should observe:
//
// - Exactly one "Sending subscribe message" log per subscription call
// - The message contains the full symbol list
// - No intermediate batching logs appear
//
// DESIGN PURPOSE
// --------------
// This example demonstrates the default behavior of the protocol session.
//
// The Immediate policy:
//
//   * preserves the user's request exactly
//   * introduces no additional scheduling or batching logic
//   * minimizes latency between API call and network transmission
//
// This policy is appropriate when:
//
//   - Requests are already within exchange limits
//   - Maximum immediacy is desired
//   - The client application performs its own batching logic
//
// ============================================================================

#include "wirekrak/core/transport/websocket_concept.hpp"
#include "wirekrak/core/transport/winhttp/websocket.hpp"
#include "wirekrak/core/protocol/kraken/session.hpp"
#include "wirekrak/core/preset/control_ring_default.hpp"
#include "lcr/buffer/managed_slot.hpp"
#include "lcr/buffer/managed_spsc_ring.hpp"
#include "lcr/memory/block_pool.hpp"

#include "common/run_multi_subscription_example.hpp"


constexpr static std::size_t BLOCK_SIZE =      128 * 1024;  // 128 KiB
constexpr static std::size_t BLOCK_COUNT =             16;  // Number of blocks in the pool
constexpr static std::size_t MESSAGE_RING_CAPACITY = 2048;  // Number of messages the ring can hold

// -------------------------------------------------------------------------
// Session setup
// -------------------------------------------------------------------------

using MyWebSocketPolicies =
    policy::transport::websocket_bundle<
        policy::transport::backpressure::Custom<32, 1, 16>  // <Spins, ActivationThreshold, DeactivationThreshold>
    >;

using MySessionPolicies =
    policy::protocol::session_bundle<
        policy::protocol::backpressure::Custom<(1 << 24)>  // <EscalationThreshold>
    >;

using MyMessageRing =
        lcr::buffer::managed_spsc_ring<
            lcr::buffer::managed_slot<1000>,
            lcr::memory::block_pool,
            MESSAGE_RING_CAPACITY
        >;

using MyWebSocket =
        wirekrak::core::transport::winhttp::WebSocketImpl<
            wirekrak::core::preset::DefaultControlRing,
            MyMessageRing,
            MyWebSocketPolicies
        >;
// Assert that MyWebSocket conforms to transport::WebSocketConcept concept
static_assert(wirekrak::core::transport::WebSocketConcept<MyWebSocket>);


using MySession =
    protocol::kraken::Session<
        MyWebSocket,
        MyMessageRing,
        MySessionPolicies
    >;

// -------------------------------------------------------------------------
// Global memory block pool
// -------------------------------------------------------------------------
static lcr::memory::block_pool memory_pool(BLOCK_SIZE, BLOCK_COUNT);


// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main(int argc, char** argv) {
    return run_multi_subscription_example<MySession, MyMessageRing>(
        argc,
        argv,
        "Wirekrak Core - Immediate Request Policy Example\n"
        "Demonstrates default request behavior without batching.\n",
        memory_pool
    );
}

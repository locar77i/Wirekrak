#include "wirekrak/core/transport/websocket/engine.hpp"
#include "wirekrak/core/transport/websocket_concept.hpp"
#include "wirekrak/core/protocol/kraken/session.hpp"
#include "wirekrak/core/preset/control_ring_default.hpp"
#include "wirekrak/core/preset/transport/backend_default.hpp"
#include "lcr/buffer/managed_slot.hpp"
#include "lcr/buffer/managed_spsc_ring.hpp"
#include "lcr/memory/block_pool.hpp"
#include "lcr/log/logger.hpp"

#include "common/run_multi_subscription.hpp"


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
        transport::websocket::Engine<
            preset::DefaultControlRing,
            MyMessageRing,
            MyWebSocketPolicies,
            preset::transport::DefaultBackend
        >;
// Assert that MyWebSocket conforms to transport::WebSocketConcept concept
static_assert(transport::WebSocketConcept<MyWebSocket>);


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
    using namespace lcr::log;
    Logger::instance().set_level(Level::Debug);

    return run_multi_subscription<MySession, MyMessageRing>(
        argc,
        argv,
        "Wirekrak Core - Multi Book/Trade Subscription Benchmark\n"
        "Test the system's ability to handle multiple high-throughput data streams with maximum depth.\n",
        memory_pool
    );
}

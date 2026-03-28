
#include "wirekrak/core/preset/message_ring_default.hpp"
#include "wirekrak/core/preset/protocol/kraken_default.hpp"
#include "lcr/memory/block_pool.hpp"
#include "lcr/log/logger.hpp"

#include "common/run_single_book_subscription.hpp"



// -------------------------------------------------------------------------
// Session setup
// -------------------------------------------------------------------------

using MyMessageRing = preset::DefaultMessageRing;
using MySession = preset::protocol::kraken::DefaultSession;

// -------------------------------------------------------------------------
// Global memory block pool
// -------------------------------------------------------------------------
static lcr::memory::block_pool memory_pool((1 << 17), 4);   // BLOCK_SIZE = 128 KiB, BLOCK_COUNT = 4 (Number of blocks in the pool)


// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main(int argc, char** argv) {
    using namespace lcr::log;
    Logger::instance().set_level(Level::Debug);

    return run_single_book_subscription<MySession, MyMessageRing>(
        argc,
        argv,
        "Wirekrak Core - Single Book Subscription Benchmark\n"
        "Test the system's ability to handle a single high-throughput data stream with maximum depth.\n",
        memory_pool,
        "USUAL/USD"
    );
}

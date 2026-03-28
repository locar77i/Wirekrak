#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>

#include "wirekrak/core.hpp"
#include "lcr/memory/block_pool.hpp"
#include "common/loop/helpers.hpp"
#include "common/kraken_pairs.hpp"


using namespace wirekrak::core;

// -----------------------------------------------------------------------------
// Lifecycle control
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}


// -----------------------------------------------------------------------------
// Generic runner
// -----------------------------------------------------------------------------
template<typename Session, typename MessageRing, typename SymbolList>
int run_multi_subscription(int argc, char** argv, const char* title, lcr::memory::block_pool& memory_pool, SymbolList symbols) {
    using namespace protocol::kraken::schema;

    // -------------------------------------------------------------------------
    // Signal handling (explicit lifecycle control)
    // -------------------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    char description[] =    "This example runs indefinitely until interrupted.\n"
                            "Press Ctrl+C to unsubscribe and exit cleanly.\n"
                            "Let's enjoy trading with Wirekrak!";

    const char* url = "wss://ws.kraken.com/v2"; // Kraken WebSocket API V2 endpoint

    // -----------------------------------------------------------------------------
    // Golbal SPSC ring buffer (transport → session)
    // -----------------------------------------------------------------------------
    static MessageRing message_ring(memory_pool);

    // -------------------------------------------------------------------------
    // Session
    // -------------------------------------------------------------------------
    Session session(message_ring);

    // -------------------------------------------------------------------------
    // Dump the session configuration (policies)
    // -------------------------------------------------------------------------
    std::cout << "\n[1] Session Configuration >>\n";
    Session::dump_configuration(std::cout);

    // -------------------------------------------------------------------------
    // Dump message ring memory usage
    // -------------------------------------------------------------------------
    std::cout << "\n[2] Message Ring Memory Usage >>\n";
    message_ring.memory_usage().debug_dump(std::cout);

    // -------------------------------------------------------------------------
    // Dump block pool memory usage
    // -------------------------------------------------------------------------
    std::cout << "\n[3] Block Pool Memory Usage >>\n";
    memory_pool.memory_usage().debug_dump(std::cout);

    // -------------------------------------------------------------------------
    // Dump session memory usage
    // -------------------------------------------------------------------------
    std::cout << "\n[4] Session Memory Usage >>\n";
    session.memory_usage().debug_dump(std::cout);

    // -------------------------------------------------------------------------
    // Connect
    // -------------------------------------------------------------------------
    std::cout << "\n[5] Running ...\n\n";
    if (!session.connect(url)) {
        return -1;
    }

    // Subscription parameters
    std::size_t depth = 1000; // Use max depth for this example
    bool snapshot = true; // Request snapshot for this example

    // -------------------------------------------------------------------------
    // Explicit subscriptions
    // -------------------------------------------------------------------------
    (void)session.subscribe(
        book::Subscribe{ .symbols = symbols, .depth = depth, .snapshot = snapshot }
    );

    (void)session.subscribe(
        trade::Subscribe{ .symbols = symbols, .snapshot = snapshot }
    );

    // -------------------------------------------------------------------------
    // Poll-driven execution loop
    // -------------------------------------------------------------------------
    // NOTE:
    // Under Strict policy, sustained transport backpressure will escalate after N
    // consecutive overloaded polls. This example intentionally stresses the system.
    int idle_spins = 0;
    bool did_work = false;
    while (running.load(std::memory_order_relaxed) && session.is_active()) {
        (void)session.poll();
        did_work = loop::drain_messages(session);
        // Yield to avoid busy-waiting when idle
        loop::manage_idle_spins(did_work, idle_spins);
    }

    // -------------------------------------------------------------------------
    // Explicit unsubscription
    // -------------------------------------------------------------------------
    if (session.is_connected()) {
        (void)session.unsubscribe(
            book::Unsubscribe{ .symbols = symbols, .depth = depth }
        );
        (void)session.unsubscribe(
            trade::Unsubscribe{ .symbols = symbols }
        );
    }

    // -------------------------------------------------------------------------
    // Graceful shutdown: drain until protocol is idle and close session
    // -------------------------------------------------------------------------
    while (!session.is_idle()) {
        (void)session.poll();
        loop::drain_messages(session);
        std::this_thread::yield();
    }

    session.close();

    // -------------------------------------------------------------------------
    // Dump telemetry
    // -------------------------------------------------------------------------
    std::cout << "\n[6] Session Telemetry >>\n";
    session.telemetry().debug_dump(std::cout);

    std::cout << "\n[SUCCESS] Clean shutdown completed.\n";
    return 0;
}

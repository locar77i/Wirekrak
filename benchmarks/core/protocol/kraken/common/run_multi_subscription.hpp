#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>

#include "wirekrak/core.hpp"
#include "wirekrak/core/perf/report.hpp"
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
        auto drained = session.data_plane().drain_all(
            [&](auto&&) noexcept {
                did_work = true;
            }
        );
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
    while (true) {
        // Fast path: if session is quiescent, we can exit immediately
        if (session.is_quiescent()) {
            break; // clean completion
        }
        // Timeout fallback (heuristic)
        if (session.is_idle()) {
            WK_WARN("[MAIN] Shutdown timeout: session did not reach quiescence. Forcing close.");
            session.subscription_controller().for_each_manager([&]<class T>(const auto& mgr) {
                std::cerr << "Manager<" << typeid(T).name() << ">\n";
                std::cerr << "  pending reqs: " << mgr.pending_requests() << "\n";
                std::cerr << "  pending syms: " << mgr.pending_symbols() << "\n";
                std::cerr << "  active syms : " << mgr.active_symbols() << "\n";
            });
            break;
        }
        // Poll and drain messages to make progress towards quiescence
        (void)session.poll();
        auto drained = session.data_plane().drain_all(
            [&](auto&&) noexcept {
                did_work = true;
            }
        );
        if (!did_work) {
            std::this_thread::yield();
        }
    }

    session.close();

    // -------------------------------------------------------------------------
    // Performance Report
    // -------------------------------------------------------------------------
    std::cout << "\n[6] Performance Report >>\n";
    perf::Report report(session.telemetry());
    report.dump(std::cout);

    std::cout << "\n[SUCCESS] Clean shutdown completed.\n";
    return 0;
}

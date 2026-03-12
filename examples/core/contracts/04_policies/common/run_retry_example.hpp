// ============================================================================
// Wirekrak Core - Generic Example Runner
// ============================================================================
//
// PURPOSE
// -------
// This header provides a reusable, policy-agnostic execution harness for
// Wirekrak Core contract examples.
//
// Instead of duplicating boilerplate across examples (signal handling,
// connection lifecycle, poll loop, subscription management, shutdown),
// this runner encapsulates the common execution pattern and delegates
// behavior differences to the injected Session type.
//
// DESIGN INTENT
// -------------
// - The Session type is fully policy-composed.
// - The runner does not know which backpressure, transport, or protocol
//   policies are active.
// - The runner only drives the Core lifecycle:
//       connect → subscribe → poll loop → unsubscribe → drain → close
//
// This separation demonstrates a key architectural principle:
//
//     Execution is stable.
//     Behavior is injected via policy composition.
//
// CONTRACT MODEL
// --------------
// - poll() is the sole execution driver.
// - Control-plane and data-plane remain deterministic and pull-based.
// - Shutdown is explicit and drain-safe.
// - No callbacks. No hidden threads. No reentrancy.
//
// USAGE
// -----
// Each example defines:
//
//   using MySession = protocol::kraken::Session<...>;
//   using MyMessageRing = lcr::lockfree::spsc_ring<...>
//
// And simply calls:
//
//   return run_example<MySession, MyMessageRing>(argc, argv, title, description);
//
// This makes each example a pure policy demonstration.
//
// ============================================================================ 
#include <chrono>
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

#include "wirekrak/core.hpp"
#include "lcr/memory/block_pool.hpp"
#include "common/cli/minimal.hpp"
#include "common/loop/helpers.hpp"

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
template<typename Session, typename MessageRing>
int run_retry_example(int argc, char** argv, const char* title) {
    using namespace protocol::kraken::schema;

    // -------------------------------------------------------------------------
    // Signal handling (explicit lifecycle control)
    // -------------------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    char description[] =    "This example cannot be terminated via Ctrl+C\n"
                            "The only exit path is a real disconnect followed by a successful reconnect.\n"
                            "Let's enjoy trading with Wirekrak!";

    // -------------------------------------------------------------------------
    // Runtime configuration (symbols, log level)º
    // -------------------------------------------------------------------------
    const auto& params = wirekrak::cli::minimal::configure(argc, argv, title, description);

    // Dump runtime parameters for observability
    params.dump("=== Runtime Parameters ===", std::cout);

    // Dump the session configuration (policies) for observability
    Session::dump_configuration(std::cout);

    // Define a list of high-volume symbols to stress the system.
    // The example will subscribe to all of them, which may trigger different policies
    // depending on the configuration.
    // Adjust the list as needed to explore different scenarios.
    const RequestSymbols symbols = {"BTC/EUR"};

    // -------------------------------------------------------------------------
    // Global memory block pool
    // -------------------------------------------------------------------------
    constexpr static std::size_t BLOCK_SIZE = 128 * 1024; // 128 KiB
    constexpr static std::size_t BLOCK_COUNT = 24;
    static lcr::memory::block_pool memory_pool(BLOCK_SIZE, BLOCK_COUNT);

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
    if (!session.connect(params.url)) {
        return -1;
    }

    // Subscription parameters
    bool snapshot = false; // Request no snapshot to keep the replay output manageable and observable

    // -------------------------------------------------------------------------
    // Subscribe ONCE to trade updates (no snapshot)
    // (no snapshot to avoid burst output and keep replay observable)
    // -------------------------------------------------------------------------
    (void)session.subscribe(
        trade::Subscribe{ .symbols  = symbols, .snapshot = snapshot }
    );

    // -------------------------------------------------------------------------
    // Main polling loop (runs until reconnect observed)
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Disconnect your network to simulate a transport failure.\n";
    std::cout << "[example] Reconnect it to observe retry / replay behavior.\n\n";

    // Wait for a few transport lifetimes to prove rejection is not replayed
    int idle_spins = 0;
    bool did_work = false;
    auto epoch = session.transport_epoch();
    while (epoch == 1 && session.is_active()) {
        epoch = session.poll();
        did_work = loop::drain_and_print_messages(session);
        loop::manage_idle_spins(did_work, idle_spins);
    }

    // -------------------------------------------------------------------------
    // Post-disconnect observation window
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Observing the retry / replay behavior...\n";

    while (running.load(std::memory_order_relaxed) && session.is_active()) {
        (void)session.poll();
        did_work = loop::drain_and_print_messages(session);
        loop::manage_idle_spins(did_work, idle_spins);
    }

    // -------------------------------------------------------------------------
    // Explicit unsubscription
    // -------------------------------------------------------------------------
    if (session.is_connected()) {
        (void)session.unsubscribe(
            trade::Unsubscribe{ .symbols = symbols }
        );
    }

    // -------------------------------------------------------------------------
    // Graceful shutdown: drain until protocol is idle and close session
    // -------------------------------------------------------------------------
    while (!session.is_idle()) {
        (void)session.poll();
        loop::drain_and_print_messages(session);
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

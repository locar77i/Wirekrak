// ============================================================================
// Wirekrak Core - Liveness Policy Demonstration Runner
// ============================================================================
//
// PURPOSE
// -------
// This runner isolates and demonstrates transport and protocol liveness
// behavior using ZERO subscriptions.
//
// By issuing no subscriptions, the system remains idle after connection,
// allowing liveness policies to activate deterministically.
//
// DESIGN INTENT
// -------------
// - No market subscriptions
// - No protocol traffic
// - No artificial activity
// - Pure liveness observation
//
// The execution loop remains identical to other examples.
// Only injected policies change behavior.
//
// This demonstrates:
//
//     Execution is stable.
//     Behavior is policy-driven.
//
// EXPECTED BEHAVIOR
// -----------------
// Depending on policy configuration:
//
// 1) Transport Liveness Disabled
//      → Nothing happens.
//
// 2) Transport Liveness Enabled + Protocol Passive
//      → LivenessThreatened
//      → LivenessExpired
//      → Connection closes
//      → Reconnect cycle begins
//
// 3) Transport Liveness Enabled + Protocol Active
//      → LivenessThreatened
//      → Session sends ping()
//      → Liveness restored
//      → No reconnect
//
// USAGE
// -----
// using MySession = protocol::kraken::Session<...>;
//
// return run_liveness_example<MySession, MyMessageRing>(argc, argv, title);
//
// ============================================================================
#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>

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
int run_zero_subscriptions_example(int argc, char** argv, const char* title, lcr::memory::block_pool& memory_pool) {
    using namespace protocol::kraken::schema;

    // -------------------------------------------------------------------------
    // Signal handling (explicit lifecycle control)
    // -------------------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    char description[] =    "This example runs indefinitely until interrupted.\n"
                            "Press Ctrl+C to unsubscribe and exit cleanly.\n"
                            "Let's enjoy trading with Wirekrak!";

    // -------------------------------------------------------------------------
    // Runtime configuration (symbols, log level)º
    // -------------------------------------------------------------------------
    const auto& params = wirekrak::cli::minimal::configure(argc, argv, title, description);

    // Dump runtime parameters for observability
    params.dump("=== Runtime Parameters ===", std::cout);

    // Dump the session configuration (policies) for observability
    Session::dump_configuration(std::cout);

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
        did_work = loop::drain_and_print_messages(session);
        // Yield to avoid busy-waiting when idle
        loop::manage_idle_spins(did_work, idle_spins);
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

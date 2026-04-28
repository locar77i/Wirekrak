// ============================================================================
// Core Contracts Example - Protocol Liveness Policy (Kraken Session)
//
// This example demonstrates how Wirekrak enforces connection liveness
// while delegating responsibility for maintaining it to the protocol layer.
//
// CONTRACT DEMONSTRATED:
//
// - transport::Connection enforces liveness *by observation*
// - protocol::kraken::Session decides whether and how to emit traffic
// - Liveness behavior is policy-driven (Passive vs Active)
// - Forced reconnects are intentional, observable, and recoverable
//
// POLICIES:
//
//   Passive:
//     - Session observes liveness only
//     - No protocol heartbeats are emitted
//     - Connection may force reconnects
//
//   Active:
//     - Session reacts to LivenessThreatened events
//     - Protocol-level pings are emitted explicitly
//     - Liveness is preserved only if traffic is observed
//
// ============================================================================

#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

#include "wirekrak/core/protocol/session.hpp"
#include "wirekrak/core/protocol/kraken_model.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"
#include "wirekrak/core/preset/transport/websocket_default.hpp"
#include "lcr/memory/block_pool.hpp"
#include "common/cli/minimal.hpp"


// -----------------------------------------------------------------------------
// Lifecycle control
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}

// -----------------------------------------------------------------------------
// Setup environment
// -----------------------------------------------------------------------------
using namespace wirekrak::core;
using namespace wirekrak::core::protocol::kraken;

// -------------------------------------------------------------------------
// Global memory block pool
// -------------------------------------------------------------------------
inline constexpr static std::size_t BLOCK_SIZE = 128 * 1024; // 128 KiB
inline constexpr static std::size_t BLOCK_COUNT = 8;
static lcr::memory::block_pool memory_pool(BLOCK_SIZE, BLOCK_COUNT);

// -----------------------------------------------------------------------------
// Golbal SPSC ring buffer (transport → session)
// -----------------------------------------------------------------------------
static preset::DefaultMessageRing message_ring(memory_pool);


// -----------------------------------------------------------------------------
// Session Type Definitions (Compile-Time Policy Injection)
// -----------------------------------------------------------------------------

using PassivePolicies =
    policy::protocol::session_bundle<
        policy::protocol::DefaultBackpressure,
        policy::protocol::liveness::Passive
    >;

using ActivePolicies =
    policy::protocol::session_bundle<
        policy::protocol::DefaultBackpressure,
        policy::protocol::liveness::Active
    >;

using PassiveSession =
    protocol::Session<
        protocol::KrakenModel,
        preset::transport::DefaultWebSocket,
        preset::DefaultMessageRing,
        PassivePolicies
    >;

using ActiveSession =
    protocol::Session<
        protocol::KrakenModel,
        preset::transport::DefaultWebSocket,
        preset::DefaultMessageRing,
        ActivePolicies
    >;


// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    using namespace schema;
    using namespace policy::protocol;

    // -------------------------------------------------------------------------
    // Runtime configuration
    // -------------------------------------------------------------------------
    const auto& params = wirekrak::cli::minimal::configure(argc, argv,
        "Wirekrak Core - Kraken Session Liveness Policy\n"
        "Demonstrates Passive vs Active protocol liveness handling.\n",
        "Passive: no protocol heartbeats, reconnects may occur.\n"
        "Active : protocol emits pings to maintain liveness.\n"
    );
    params.dump("=== Runtime Parameters ===", std::cout);

    // -------------------------------------------------------------------------
    // Signal handling (explicit termination)
    // -------------------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------------------
    // Phase A - Passive liveness
    // -------------------------------------------------------------------------
    {
        std::cout << "\n[example] Phase A - Passive liveness policy\n";
        std::cout << "          No protocol heartbeats will be sent.\n";
        std::cout << "          Forced reconnects are expected if the protocol remains silent.\n\n";

        // Session with passive liveness policy: connection may force reconnects if no traffic is observed
        PassiveSession session{message_ring};

        if (!session.connect(params.url)) {
            return -1;
        }

        // Poll-driven execution loop
        std::uint64_t epoch = session.transport_epoch();
        while (epoch < 2) { // Run until 2 forced reconnects occur (for demonstration purposes)
            epoch = session.poll();
            // Observe latest pong (liveness signal - not relevant in Passive policy)
            if (const auto* pong = session.data_plane().get<system::Pong>()) {
                std::cout << " -> " << *pong << std::endl;
            }
            std::this_thread::yield();
        }

        // Shutdown
        session.close();

        // Dump telemetry
        session.telemetry().debug_dump(std::cout);
    }
    // -------------------------------------------------------------------------
    // Phase B - Active liveness
    // -------------------------------------------------------------------------
    {
        std::cout << "\n[example] Phase B - Active liveness policy\n";
        std::cout << "          Session will react to liveness warnings\n";
        std::cout << "          by sending protocol-level pings.\n\n";

        // Session with active liveness policy: session will attempt to maintain liveness by emitting protocol pings
        ActiveSession session{message_ring};

        if (!session.connect(params.url)) {
            return -1;
        }

        // Poll-driven execution loop
        while (running.load(std::memory_order_relaxed)) {
            (void)session.poll();
            // Observe latest pong (liveness signal - only relevant in Active policy)
            if (const auto* pong = session.data_plane().get<system::Pong>()) {
                std::cout << " -> " << *pong << std::endl;
            }
            std::this_thread::yield();
        }

        // Graceful shutdown: drain until protocol is idle and close session
        while (!session.is_idle()) {
            (void)session.poll();
            std::this_thread::yield();
        }

        session.close();

        // Dump telemetry
        session.telemetry().debug_dump(std::cout);
    }

    // =========================================================================
    // Summary
    // =========================================================================
    std::cout << "\n=== Summary ===\n"
              << "- Liveness policy is compile-time injected.\n"
              << "- Passive does not emit pings.\n"
              << "- Active emits protocol-level pings.\n"
              << "- No runtime policy switching.\n"
              << "- Deterministic and zero-overhead.\n\n";

    return 0;
}

// ============================================================================
// Lite example 03_lifecycle
//
// Demonstrates:
// - Connecting and running a Lite client
// - Graceful shutdown using Ctrl+C
// - Draining in-flight events before exit
//
// No subscription management or advanced behavior is introduced.
// ============================================================================

#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>

#include "wirekrak.hpp"

#include "common/logger.hpp"

// -----------------------------------------------------------------------------
// Ctrl+C handling
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main() {
    using namespace wirekrak::lite;

    wirekrak::log::set_level("debug");

    // -------------------------------------------------------------
    // Signal handling
    // -------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------
    // Client setup
    // -------------------------------------------------------------
    Client client{"wss://ws.kraken.com/v2"};

    if (!client.connect()) {
        std::cerr << "[wirekrak-lite] Failed to connect\n";
        return -1;
    }

    std::cout << "[wirekrak-lite] Client running. Press Ctrl+C to exit.\n";

    // -------------------------------------------------------------
    // Main polling loop
    // -------------------------------------------------------------
    while (running.load(std::memory_order_relaxed)) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[wirekrak-lite] Shutting down...\n";

    // Optional explicit disconnect (safe to call)
    client.disconnect();

    std::cout << "[wirekrak-lite] Done.\n";
    return 0;
}

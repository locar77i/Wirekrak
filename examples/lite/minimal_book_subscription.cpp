#include <iostream>
#include <chrono>
#include <csignal>
#include <atomic>
#include <thread>

// Lite v1 invariant:
// - Each callback corresponds to one price level update
// - snapshot delivers full depth
// - update delivers incremental changes
#include "wirekrak/lite.hpp"
using namespace wirekrak::lite;

// -----------------------------------------------------------------------------
// Ctrl+C handling
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}

int main() {
    std::signal(SIGINT, on_signal);  // Handle Ctrl+C

    // -------------------------------------------------------------------------
    // Client setup
    // -------------------------------------------------------------------------
    Client client{"wss://ws.kraken.com/v2"};

    if (!client.connect()) {
        std::cerr << "[wirekrak-lite] Failed to connect\n";
        return -1;
    }

    // -------------------------------------------------------------------------
    // Subscribe to BTC/EUR book updates
    // -------------------------------------------------------------------------
    int messages_received = 0;

    client.subscribe_book(
        {"BTC/EUR"},
        [&](const dto::book_level& lvl) {
            std::cout << " -> " << lvl << std::endl;
            ++messages_received;
        },
        true   // snapshot
    );

    // -------------------------------------------------------------------------
    // Main polling loop (runs until Ctrl+C)
    // -------------------------------------------------------------------------
    while (running.load(std::memory_order_relaxed) && messages_received < 60) {
        client.poll();   // REQUIRED to process incoming messages
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Unsubscribe & exit
    // -------------------------------------------------------------------------
    client.unsubscribe_book({"BTC/EUR"});

    std::cout << "\n[wirekrak-lite] Done.\n";
    return 0;
}

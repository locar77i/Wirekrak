#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>

// Lite v1 invariant:
// - Each callback corresponds to exactly one trade
// - origin indicates snapshot vs live update
// - ordering is preserved per symbol
#include "wirekrak/lite.hpp"
using namespace wirekrak::lite;

#include "common/cli/trade_params.hpp"
namespace cli = wirekrak::examples::cli;


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
int main(int argc, char** argv) {
    // -------------------------------------------------------------
    // Signal handling
    // -------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------
    // CLI parsing
    // -------------------------------------------------------------
    const auto& params = cli::trade::configure(argc, argv, "WireKrak Lite - Kraken Trade Subscription Example\n"
        "This example let's you subscribe to trade events on a given symbol from Kraken WebSocket API v2.\n"
    );
    params.dump("=== Trade Example Parameters ===", std::cout);

    // -------------------------------------------------------------
    // Client setup
    // -------------------------------------------------------------
    Client client{params.url};

    client.on_error([](const error& err) {
        std::cerr << "[wirekrak-lite] error: " << err.message << "\n";
    });

    if (!client.connect()) {
        std::cerr << "[wirekrak-lite] Failed to connect\n";
        return -1;
    }

    // -------------------------------------------------------------
    // Trade subscription
    // -------------------------------------------------------------
    auto trade_handler = [](const dto::trade& t) {
        std::cout << " -> " << t << std::endl;
    };

    client.subscribe_trades(params.symbols, trade_handler, params.snapshot);

    // -------------------------------------------------------------
    // Main polling loop (runs until Ctrl+C)
    // -------------------------------------------------------------
    while (running.load()) {
        client.poll();   // REQUIRED to process incoming messages
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------
    // Unsubscribe from trade updates
    // -------------------------------------------------------------
    client.unsubscribe_trades(params.symbols);

    // Drain events before exit (approx. 2 seconds)
    for (int i = 0; i < 200; ++i) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\n[wirekrak-lite] Done.\n";
    return 0;
}

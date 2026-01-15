#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>

#include "wirekrak/lite.hpp"
using namespace wirekrak::lite;

#include "common/cli/book_params.hpp"
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
    const auto& params = cli::book::configure(argc, argv, "WireKrak Core - Kraken Book Subscription Example\n"
        "This example let's you subscribe to book events on a given symbol from Kraken WebSocket API v2.\n"
    );
    params.dump("=== Book Example Parameters ===", std::cout);

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
    // Subscribe to book updates
    // -------------------------------------------------------------
    auto book_handler = [](const dto::book_level& lvl) {
        std::cout << " -> " << lvl << std::endl;
    };

    client.subscribe_book(params.symbols, book_handler, params.snapshot);

    // -------------------------------------------------------------
    // Main polling loop (runs until Ctrl+C)
    // -------------------------------------------------------------
    while (running.load()) {
        client.poll();   // REQUIRED to process incoming messages
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------
    // Unsubscribe from book updates
    // -------------------------------------------------------------
    client.unsubscribe_book(params.symbols);

    // Drain events before exit (approx. 2 seconds)
    for (int i = 0; i < 200; ++i) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\n[wirekrak-lite] Done.\n";
    return 0;
}

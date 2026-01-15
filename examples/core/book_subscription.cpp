#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>
#include <regex>

#include <CLI/CLI.hpp>

#include "wirekrak/client.hpp"

#include "common/cli/book_params.hpp"

namespace cli = wirekrak::examples::cli;
using namespace wirekrak;
using namespace wirekrak::protocol::kraken;

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
    WinClient client;

    // Register pong handler
    client.on_pong([&](const schema::system::Pong& pong) {
        WK_INFO(" -> " << pong << "");
    });

    // Register status handler
    client.on_status([&](const schema::status::Update& update) {
        WK_INFO(" -> " << update << "");
    });

    // Register regection handler
    client.on_rejection([&](const schema::rejection::Notice& notice) {
        WK_WARN(" -> " << notice << "");
    });

    // Connect
    if (!client.connect(params.url)) {
        return -1;
    }

    // -------------------------------------------------------------
    // Subscribe to book updates
    // -------------------------------------------------------------
    client.subscribe(schema::book::Subscribe{.symbols = params.symbols, .depth = params.depth, .snapshot = params.snapshot},
                     [](const schema::book::Response& msg) {
                        std::cout << " -> " << msg << std::endl;
                     }
    );

    // -------------------------------------------------------------------------
    // Main polling loop (runs until Ctrl+C)
    // -------------------------------------------------------------------------
    while (running.load()) {
        client.poll();   // REQUIRED to process incoming messages
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Unsubscribe from book updates
    // -------------------------------------------------------------------------
    client.unsubscribe(schema::book::Unsubscribe{.symbols = params.symbols, .depth = params.depth});

    // Drain events before exit (approx. 2 seconds)
    for (int i = 0; i < 200; ++i) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "=== Done ===\n";
    return 0;
}

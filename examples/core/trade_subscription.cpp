#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>
#include <regex>

#include "wirekrak/core.hpp"
using namespace wirekrak::core;
namespace schema = protocol::kraken::schema;

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
    const auto& params = cli::trade::configure(argc, argv, "Wirekrak Core - Kraken Trade Subscription Example\n"
        "This example let's you subscribe to trade events on a given symbol from Kraken WebSocket API v2.\n"
    );
    params.dump("=== Trade Example Parameters ===", std::cout);

    // -------------------------------------------------------------
    // Session setup
    // -------------------------------------------------------------
    Session session;

    // Register pong handler
    session.on_pong([&](const schema::system::Pong& pong) {
        WK_INFO(" -> " << pong << "");
    });

    // Register status handler
    session.on_status([&](const schema::status::Update& update) {
        WK_INFO(" -> " << update << "");
    });

    // Register regection handler
    session.on_rejection([&](const schema::rejection::Notice& notice) {
        WK_WARN(" -> " << notice << "");
    });

    // Connect
    if (!session.connect(params.url)) {
        return -1;
    }

    // -------------------------------------------------------------
    // Subscribe to trade updates
    // -------------------------------------------------------------
    session.subscribe(schema::trade::Subscribe{.symbols = params.symbols, .snapshot = params.snapshot},
                     [](const schema::trade::ResponseView& msg) {
                        std::cout << " -> " << msg << std::endl;
                     }
    );

    // -------------------------------------------------------------------------
    // Main polling loop (runs until Ctrl+C)
    // -------------------------------------------------------------------------
    while (running.load()) {
        session.poll();   // REQUIRED to process incoming messages
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Unsubscribe from trade updates
    // -------------------------------------------------------------------------
    session.unsubscribe(schema::trade::Unsubscribe{.symbols = params.symbols});

    // Drain events before exit (approx. 2 seconds)
    for (int i = 0; i < 200; ++i) {
        session.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "=== Done ===\n";
    return 0;
}

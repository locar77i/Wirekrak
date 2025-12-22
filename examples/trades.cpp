#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>
#include <regex>

#include <CLI/CLI.hpp>

#include "wirekrak/client.hpp"

using namespace wirekrak;
using namespace wirekrak::protocol::kraken;
using namespace lcr::log;

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
    // CLI parsing
    // -------------------------------------------------------------
    CLI::App app{"WireKrak - Kraken Trade Subscription Example\n"
        "This example let's you subscribe to trade events on a given symbol from Kraken WebSocket API v2.\n"};

    std::vector<std::string> symbols = {"BTC/USD"};
    std::string url                  = "wss://ws.kraken.com/v2";
    bool snapshot                    = false;
    bool double_sub                  = false;
    std::string log_level            = "info";

    auto ws_url_validator = CLI::Validator(
        [](std::string &value) -> std::string {
            if (value.rfind("ws://", 0) == 0 || value.rfind("wss://", 0) == 0) {
                return {}; // OK
            }
            return "URL must start with ws:// or wss://";
        },
        "WebSocket URL validator"
    );

    auto symbol_validator = CLI::Validator(
        [](std::string &value) -> std::string {
            if (value.find('/') != std::string::npos) {
                return {};
            }
            return "Symbol must be in format BASE/QUOTE (e.g. BTC/USD)";
        },
        "Trading symbol validator"
    );

    app.add_option("--url", url, "Kraken WebSocket URL")->check(ws_url_validator)->default_val(url);
    app.add_option("-s,--symbol", symbols, "Trading symbol(s), repeatable (e.g. -s BTC/USD -s ETH/USD)")->check(symbol_validator)->default_val(symbols);
    app.add_option("--snapshot", snapshot, "Request trade snapshot")->check(CLI::IsMember({true, false}))->default_val(snapshot);
    app.add_flag("--double-sub", double_sub, "Subscribe twice to demonstrate rejection handling");
    app.add_option("-l, --log-level", log_level, "Log level: trace | debug | info | warn | error")->default_val(log_level);
    app.footer(
        "This example runs indefinitely until interrupted.\n"
        "Press Ctrl+C to unsubscribe and exit cleanly.\n"
        "Let's enjoy trading with WireKrak!"
    );

    CLI11_PARSE(app, argc, argv);

    // -------------------------------------------------------------
    // Logging
    // -------------------------------------------------------------
    if (log_level == "trace") Logger::instance().set_level(Level::Trace);
    else if (log_level == "debug") Logger::instance().set_level(Level::Debug);
    else if (log_level == "warn")  Logger::instance().set_level(Level::Warn);
    else if (log_level == "error") Logger::instance().set_level(Level::Error);
    else                           Logger::instance().set_level(Level::Info);

    // -------------------------------------------------------------
    // Signal handling
    // -------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    std::cout << "=== WireKrak Trade Example ===\n"
              << "Symbols  : ";
    for (const auto& s : symbols) { std::cout << s << " "; }
    std::cout << "\n"
              << "Snapshot : " << (snapshot ? "true" : "false") << "\n"
              << "URL      : " << url << "\n"
              << "Press Ctrl+C to exit\n\n";

    // -------------------------------------------------------------
    // Client setup
    // -------------------------------------------------------------
    WinClient client;

    // Register pong handler
    client.on_pong([&](const system::Pong& pong) {
        WK_INFO(" -> " << pong.str() << "");
    });

    // Register status handler
    client.on_status([&](const status::Update& update) {
        WK_INFO(" -> " << update.str() << "");
    });

    // Register regection handler
    client.on_rejection([&](const rejection::Notice& notice) {
        WK_WARN(" -> " << notice.str() << "");
    });

    // Connect
    if (!client.connect(url)) {
        return -1;
    }

    // Subscribe to BTC/USD trades with snapshot enabled
    client.subscribe(trade::Subscribe{.symbols = symbols, .snapshot = snapshot},
                     [](const trade::Trade& msg) {
                        std::cout << " -> " << msg << std::endl;
                     }
    );

    if (double_sub) {
        // Subscribe again to demonstrate rejection handling
        client.subscribe(trade::Subscribe{.symbols = symbols, .snapshot = snapshot},
                         [](const trade::Trade& msg) {
                            std::cout << " -> " << msg << std::endl;
                         }
        );
    }

    // Main polling loop
    while (running.load()) {
        client.poll();   // REQUIRED to process incoming messages
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Ctrl+C received
    client.unsubscribe(trade::Unsubscribe{.symbols = symbols});
    if (double_sub) {
        client.unsubscribe(trade::Unsubscribe{.symbols = symbols});
    }

    // Drain events
    for (int i = 0; i < 200; ++i) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "=== Done ===\n";
    return 0;
}

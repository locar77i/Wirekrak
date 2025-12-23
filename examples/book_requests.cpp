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
    CLI::App app{"WireKrak - Kraken Book Subscription Example\n"
        "This example let's you subscribe to book updates on a given symbol from Kraken WebSocket API v2.\n"};

    std::vector<std::string> symbols = {"BTC/USD"};
    std::string url                  = "wss://ws.kraken.com/v2";
    std::uint32_t depth              = 10;
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

    auto depth_validator = CLI::Validator(
        [](std::string& value) -> std::string {
            try {
                auto depth = std::stoul(value);
                switch (depth) {
                    case 10:
                    case 25:
                    case 100:
                    case 500:
                    case 1000:
                        return {}; // valid
                    default:
                        return "Depth must be one of: 10, 25, 100, 500, 1000";
                }
            } catch (...) {
                return "Depth must be a valid integer";
            }
        },
        "Order book depth validator"
    );

    app.add_option("--url", url, "Kraken WebSocket URL")->check(ws_url_validator)->default_val(url);
    app.add_option("-s,--symbol", symbols, "Trading symbol(s), repeatable (e.g. -s BTC/USD -s ETH/USD)")->check(symbol_validator)->default_val(symbols);
    app.add_option("-d,--depth", depth, "Order book depth (10, 25, 100, 500, 1000)")->check(depth_validator)->default_val(depth);
    app.add_flag("--snapshot", snapshot, "Request book snapshot");
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

    std::cout << "=== WireKrak Book Example ===\n"
              << "Symbols  : ";
    for (const auto& s : symbols) { std::cout << s << " "; }
    std::cout << "\n"
              << "Depth    : " << depth << "\n"
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

    // Subscribe to BTC/USD book updates
    client.subscribe(book::Subscribe{.symbols = symbols, .depth = depth, .snapshot = snapshot},
                     [](const book::Response& msg) {
                        std::cout << " -> " << msg << std::endl;
                     }
    );

    if (double_sub) {
        // Subscribe again to demonstrate rejection handling
        client.subscribe(book::Subscribe{.symbols = symbols, .depth = depth, .snapshot = snapshot},
                     [](const book::Response& msg) {
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
    client.unsubscribe(book::Unsubscribe{.symbols = symbols, .depth = depth});
    if (double_sub) {
        client.unsubscribe(book::Unsubscribe{.symbols = symbols, .depth = depth});
    }

    // Drain events
    for (int i = 0; i < 200; ++i) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "=== Done ===\n";
    return 0;
}

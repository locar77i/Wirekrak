#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>

#include "wirekrak/winhttp/client.hpp"
#include "wirekrak/protocol/kraken/trade/subscribe.hpp"

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
int main() {
    Logger::instance().set_level(Level::Info);

    std::signal(SIGINT, on_signal);

    std::cout << "=== WireKrak Trade Snapshot Example (BTC/USD) ===\n";
    std::cout << "Press Ctrl+C to exit\n\n";

    winhttp::WinClient client;

    // Connect
    if (!client.connect("wss://ws.kraken.com/v2")) {
        return -1;
    }

    // Subscribe to BTC/USD trades with snapshot enabled
    std::cout << "[SUBSCRIBE] trade BTC/USD (snapshot=true)\n";
    client.subscribe(trade::Subscribe{.symbols = {"BTC/USD"}, .snapshot = true},
                     [](const trade::Trade& msg) {
                        std::cout << " -> [" << msg.symbol << "] TRADE: id=" << msg.trade_id << " price=" << msg.price << " qty=" << msg.qty << " side=" << to_string(msg.side) << std::endl;
                     }
    );

    // Subscribe to BTC/USD trades with snapshot enabled
    std::cout << "[SUBSCRIBE] trade BTC/USD (snapshot=true)\n";
    client.subscribe(trade::Subscribe{.symbols = {"BTC/USD"}, .snapshot = true},
                     [](const trade::Trade& msg) {
                        std::cout << " -> [" << msg.symbol << "] TRADE: id=" << msg.trade_id << std::endl;
                     }
    );

    // Main polling loop
    while (running.load()) {
        client.poll();   // REQUIRED to process incoming messages
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Ctrl+C received
    client.unsubscribe(trade::Unsubscribe{.symbols = {"BTC/USD"}});
    client.unsubscribe(trade::Unsubscribe{.symbols = {"BTC/USD"}});

    // Drain events
    for (int i = 0; i < 200; ++i) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "=== Done ===\n";
    return 0;
}

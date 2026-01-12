#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>

#include "wirekrak/client.hpp"

using namespace wirekrak;

// -----------------------------------------------------------------------------
// Ctrl+C handling
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}


int main() {
    std::signal(SIGINT, on_signal);  // Handle Ctrl+C

    WinClient client;   // 1) Create client and connect to Kraken WebSocket API v2
    if (!client.connect("wss://ws.kraken.com/v2")) {
        return -1;
    }

    int messages_received = 0;   // 2) Subscribe to BTC/EUR trades
    client.subscribe(protocol::kraken::schema::trade::Subscribe{.symbols = {"BTC/EUR"}},
                     [&](const protocol::kraken::schema::trade::ResponseView& msg) {
                            std::cout << " -> " << msg << std::endl;
                            ++messages_received;
                     }
    );

    while (running.load(std::memory_order_relaxed) && messages_received < 10) {
        client.poll();           // REQUIRED to process incoming messages
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    client.unsubscribe(protocol::kraken::schema::trade::Unsubscribe{.symbols = {"BTC/EUR"}});   // 3) Unsubscribe from BTC/EUR trades
    
    std::cout << "\n[wirekrak] Heartbeats received so far: " << client.heartbeat_total() << std::endl;
    return 0;
}

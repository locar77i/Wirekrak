#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>

#include "wirekrak/core.hpp"
using namespace wirekrak::core;
namespace schema = protocol::kraken::schema;


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
    // Session setup
    // -------------------------------------------------------------------------
    Session session;   // 1) Create session and connect to Kraken WebSocket API v2
    if (!session.connect("wss://ws.kraken.com/v2")) {   
        return -1;
    }

    // -------------------------------------------------------------------------
    // Subscribe to BTC/EUR book updates
    // -------------------------------------------------------------------------
    int messages_received = 0;   // 2) Subscribe to BTC/EUR book updates
    session.subscribe(schema::book::Subscribe{.symbols = {"BTC/EUR"}},
                     [&](const schema::book::Response& msg) {
                            std::cout << " -> " << msg << std::endl;
                            ++messages_received;
                     }
    );

    // -------------------------------------------------------------------------
    // Main polling loop (runs until Ctrl+C)
    // -------------------------------------------------------------------------
    while (running.load(std::memory_order_relaxed) && messages_received < 60) {
        session.poll();         // REQUIRED to process incoming messages
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Unsubscribe & exit
    // -------------------------------------------------------------------------
    session.unsubscribe(schema::book::Unsubscribe{.symbols = {"BTC/EUR"}});   // 3) Unsubscribe from BTC/EUR book updates
    
    std::cout << "\n[wirekrak] Heartbeats received so far: " << session.heartbeat_total() << std::endl;
    return 0;
}

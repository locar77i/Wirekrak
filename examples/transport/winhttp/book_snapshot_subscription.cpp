#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

#include "wirekrak/transport/winhttp/websocket.hpp"

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

    transport::winhttp::WebSocket ws;

    ws.set_message_callback([](const std::string& msg) {
        std::cout << "Received: " << msg << "\n\n";
    });

    ws.set_close_callback([]() {
        std::cout << "[WS] Connection closed\n";
    });

    if (!ws.connect("ws.kraken.com", "443", "/v2")) {
        std::cerr << "Connect failed\n";
        return 1;
    }

    // Give Kraken a moment to send the system status
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // -------------------------------------------------------------------------
    // Subscribe to BOOK channel with SNAPSHOT
    // -------------------------------------------------------------------------
    bool result = ws.send(R"json(
    {
        "method": "subscribe",
        "params": {
            "channel": "book",
            "symbol": ["BTC/USD"],
            "depth": 10,
            "snapshot": true
        }
    }
    )json");

    if (!result) {
        std::cerr << "Subscribe failed\n";
        return 2;
    }

    std::cout << "Subscribed to book snapshot. Waiting for messages... (Ctrl+C to exit)\n";

    // -------------------------------------------------------------------------
    // Event loop
    // -------------------------------------------------------------------------
    while (running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << "Shutting down...\n";
    ws.close();
    return 0;
}

#include <iostream>
#include <string_view>
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

    transport::telemetry::WebSocket telemetry;
    transport::winhttp::WebSocket ws(telemetry);

    ws.set_message_callback([](std::string_view msg) {
        std::cout << "Received: " << msg << "\n\n";
    });

    ws.set_close_callback([]() {
        std::cout << "[WS] Connection closed\n";
    });

    if (!ws.connect("ws.kraken.com", "443", "/v2")) {
        std::cerr << "Connect failed\n";
        return 1;
    }

    // wait a bit for messages
    std::this_thread::sleep_for(std::chrono::seconds(2));

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
    // Event loop - Keep running until interrupted
    // -------------------------------------------------------------------------
    while (running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Shutting down...\n";
    ws.close();
    return 0;
}

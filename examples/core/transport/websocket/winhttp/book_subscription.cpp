#include <iostream>
#include <string_view>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

#include "wirekrak/core/transport/winhttp/websocket.hpp"
using namespace wirekrak::core;

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

    if (ws.connect("ws.kraken.com", "443", "/v2") != transport::Error::None) {
        std::cerr << "Connect failed" << std::endl;
        return 1;
    }

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
        std::cerr << "Subscribe failed" << std::endl;
        return 2;
    }

    std::cout << "Subscribed to book snapshot. Waiting for messages... (Ctrl+C to exit)" << std::endl;

    // -------------------------------------------------------------------------
    // Event loop - Keep running until interrupted
    // -------------------------------------------------------------------------
    while (running.load(std::memory_order_relaxed)) {
        // Drain control-plane events
        transport::websocket::Event ev;
        while (ws.poll_event(ev)) {
            std::cout << "[example] Event received: " << static_cast<int>(ev.type) << std::endl;
        }
        // Drain data-plane messages (zero-copy)
        while (auto* block = ws.peek_message()) {
            std::string_view msg(block->data, block->size);
            std::cout << "Received:\n" << msg << "\n" << std::endl;
            ws.release_message();
        }
        std::this_thread::yield();
    }

    std::cout << "Shutting down..." << std::endl;
    ws.close();
    return 0;
}

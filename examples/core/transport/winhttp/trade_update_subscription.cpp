#include <iostream>
#include <string_view>
#include <chrono>
#include <thread>
#include <locale>
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

    ws.set_message_callback([](std::string_view msg){
        std::cout << "Received: " << msg << std::endl;
    });

    ws.set_close_callback([]() {
        std::cout << "[WS] Connection closed\n";
    });

    if (ws.connect("ws.kraken.com", "443", "/v2") != transport::Error::None) {
        std::cerr << "Connect failed\n";
        return 1;
    }

    // wait a bit for messages
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // -------------------------------------------------------------------------
    // Subscribe to TRADE channel
    // -------------------------------------------------------------------------
    bool result = ws.send(R"({
        "method": "subscribe",
        "params": {
            "channel": "trade",
            "symbol": ["BTC/USD"]
        }
    })");
    
    if (!result) {
        return 3;
    }

    std::cout << "Subscribed to trade updates. Waiting for messages... (Ctrl+C to exit)\n";

    // -------------------------------------------------------------------------
    // Event loop - Keep running until interrupted
    // -------------------------------------------------------------------------
    std::cout << "Subscribed. Waiting for messages... (Ctrl+C to exit)\n";
    while (running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Shutting down...\n";
    ws.close();
    return 0;
}

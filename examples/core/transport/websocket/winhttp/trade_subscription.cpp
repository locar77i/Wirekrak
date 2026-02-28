#include <iostream>
#include <string_view>
#include <chrono>
#include <thread>
#include <locale>
#include <csignal>

#include "wirekrak/core/preset/transport/websocket_default.hpp"


// -----------------------------------------------------------------------------
// Setup environment
// -----------------------------------------------------------------------------
using namespace wirekrak::core;
using namespace wirekrak::core::transport;

static preset::DefaultControlRing control_ring;   // Golbal control SPSC ring buffer (transport → session)
static preset::DefaultMessageRing message_ring;   // Golbal message SPSC ring buffer (transport → session)

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
    std::signal(SIGINT, on_signal);  // Handle Ctrl+C

    telemetry::WebSocket telemetry;
    preset::transport::DefaultWebSocket ws(control_ring, message_ring, telemetry);

    if (ws.connect("ws.kraken.com", "443", "/v2") != Error::None) {
        std::cerr << "Connect failed" << std::endl;
        return 1;
    }

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
        std::cerr << "Subscribe failed" << std::endl;
        return 2;
    }

    std::cout << "Subscribed to trade updates. Waiting for messages... (Ctrl+C to exit)" << std::endl;

    // -------------------------------------------------------------------------
    // Event loop - Keep running until interrupted
    // -------------------------------------------------------------------------
    std::cout << "Subscribed. Waiting for messages... (Ctrl+C to exit)" << std::endl;
    while (running.load(std::memory_order_relaxed)) {
        // Drain control-plane events
        websocket::Event ev;
        while (control_ring.pop(ev)) {
            std::cout << "[example] Event received: " << static_cast<int>(ev.type) << std::endl;
        }
        // Drain data-plane messages (zero-copy)
        while (auto* block = message_ring.peek_consumer_slot()) {
            std::string_view msg(block->data, block->size);
            std::cout << "Received:\n" << msg << "\n\n";
            message_ring.release_consumer_slot();
        }
        std::this_thread::yield();
    }

    std::cout << "Shutting down..." << std::endl;
    ws.close();
    return 0;
}

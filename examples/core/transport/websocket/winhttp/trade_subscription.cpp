#include <iostream>
#include <string_view>
#include <chrono>
#include <thread>
#include <locale>
#include <csignal>

#include "wirekrak/core/preset/transport/websocket_default.hpp"
#include "lcr/memory/block_pool.hpp"


// -----------------------------------------------------------------------------
// Ctrl+C handling
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}

// -----------------------------------------------------------------------------
// Setup environment
// -----------------------------------------------------------------------------
using namespace wirekrak::core;
using namespace wirekrak::core::transport;

// -----------------------------------------------------------------------------
// Golbal control SPSC ring buffer (transport → session)
// -----------------------------------------------------------------------------
static preset::DefaultControlRing control_ring;

// -------------------------------------------------------------------------
// Global memory block pool
// -------------------------------------------------------------------------
inline constexpr static std::size_t BLOCK_SIZE = 128 * 1024; // 128 KiB
inline constexpr static std::size_t BLOCK_COUNT = 8;
static lcr::memory::block_pool memory_pool(BLOCK_SIZE, BLOCK_COUNT);

// -----------------------------------------------------------------------------
// Golbal SPSC ring buffer (transport → session)
// -----------------------------------------------------------------------------
static preset::DefaultMessageRing message_ring(memory_pool);


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
        while (auto* slot = message_ring.peek_consumer_slot()) {
            std::string_view msg(slot->data(), slot->size());
            std::cout << "Received:\n" << msg << "\n\n";
            message_ring.release_consumer_slot();
        }
        std::this_thread::yield();
    }

    std::cout << "Shutting down..." << std::endl;
    ws.close();
    return 0;
}

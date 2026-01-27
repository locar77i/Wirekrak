#pragma once

/*
===============================================================================
Example 2 - Connection vs Transport Semantics
===============================================================================

This runner demonstrates the semantic boundary between:

  • WebSocket transport (what arrives on the wire)
  • Connection layer (what is forwarded to user code)

It shows that receiving messages does NOT imply delivery to the application.

The example runs in two phases:

  Phase A:
    - No message callback installed
    - Transport receives messages
    - Connection forwards nothing

  Phase B:
    - Message callback installed
    - Connection begins forwarding messages

This proves:
  messages_forwarded_total != messages_rx_total is correct behavior.

===============================================================================
*/

#include <iostream>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>
#include <csignal>

#include "wirekrak/core/transport/connection.hpp"
#include "wirekrak/core/transport/winhttp/websocket.hpp"

// WebSocket transport specialization
using WS = wirekrak::core::transport::winhttp::WebSocket;

// -----------------------------------------------------------------------------
// Ctrl+C handling
// -----------------------------------------------------------------------------
static std::atomic<bool> running{true};

inline void on_signal(int) {
    running.store(false);
}

// -----------------------------------------------------------------------------
// Runner
// -----------------------------------------------------------------------------

inline int run_example(const char* name, const char* url, const char* description, const char* subscribe_payload) {

    std::signal(SIGINT, on_signal);

    std::cout
        << "=== Wirekrak Connection - Transport vs Delivery (" << name << ") ===\n\n"
        << description << "\n\n";

    // -------------------------------------------------------------------------
    // Connection setup
    // -------------------------------------------------------------------------
    wirekrak::core::transport::telemetry::Connection telemetry;
    wirekrak::core::transport::Connection<WS> connection(telemetry);

    std::atomic<uint64_t> forwarded{0};

    connection.on_connect([&] {
        std::cout << "[example] Connected to " << name << " WebSocket\n";
    });

    connection.on_disconnect([&] {
        std::cout << "[example] Disconnected\n";
    });

    // -------------------------------------------------------------------------
    // Open connection
    // -------------------------------------------------------------------------
    std::cout << "[example] Connecting to " << url << "\n";
    if (connection.open(url) != wirekrak::core::transport::Error::None) {
        std::cerr << "[example] Failed to connect\n";
        return 1;
    }

    // -------------------------------------------------------------------------
    // Subscribe immediately
    // -------------------------------------------------------------------------
    std::cout << "[example] Sending subscription\n";
    (void)connection.send(subscribe_payload);

    // -------------------------------------------------------------------------
    // Phase A - no message hook
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Phase A - transport receives, connection does NOT forward\n";

    auto phase_a_start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - phase_a_start < std::chrono::seconds(10)
           && running.load(std::memory_order_relaxed)) {
        connection.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Phase B - install message hook
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Phase B - installing message callback\n";

    connection.on_message([&](std::string_view msg) {
        ++forwarded;
        std::cout << "[example] Forwarded message (" << msg.size() << " bytes)\n";
    });

    while (running.load(std::memory_order_relaxed)) {
        connection.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Close connection
    // -------------------------------------------------------------------------
    connection.close();

    // Drain remaining events (~200 ms)
    for (int i = 0; i < 20; ++i) {
        connection.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Dump telemetry
    // -------------------------------------------------------------------------
    std::cout << "\n=== Connection Telemetry ===\n";
    telemetry.debug_dump(std::cout);

    std::cout << "\n=== WebSocket Telemetry ===\n";
    telemetry.websocket.debug_dump(std::cout);

    // -------------------------------------------------------------------------
    // How to read this
    // -------------------------------------------------------------------------
    std::cout << "\n=== key insights ===\n"
        << "[RX messages]---------  observed arriving messages on the wire, regardless of application interest.\n"
        << "[Messages forwarded]--  messages intentionally delivered to user code. Only counted after a message callback exists.\n"
        << "It is expected and correct that:  [Messages forwarded] <= [RX messages]\n\n"
        << "Wirekrak separates observation (transport) from policy and delivery (connection).\n";

    return 0;
}

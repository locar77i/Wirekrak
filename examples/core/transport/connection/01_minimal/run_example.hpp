#pragma once

/*
===============================================================================
Example 1 - Message Shape & Fragmentation
===============================================================================

This runner demonstrates how Wirekrak reports WebSocket *message shape*
based on observable wire behavior - not sender intent.

It is exchange-agnostic and driven entirely by configuration data.

Scenario:
  1) Connect to a WebSocket endpoint
  2) Send a subscription message
  3) Receive messages of varying sizes
  4) Dump transport and connection telemetry

What this teaches:
  - RX messages vs framing
  - Fragments/msg meaning
  - RX fragments meaning
  - RX message bytes meaning

Key takeaway:
  Telemetry reflects what actually happened on the wire,
  not what the application or exchange intended.

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

// Websocket transport specialization
using WS = wirekrak::core::transport::winhttp::WebSocket;

// -----------------------------------------------------------------------------
// Ctrl+C handling
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}

// -----------------------------------------------------------------------------
// Runner
// -----------------------------------------------------------------------------

inline int run_example(const char* name, const char* url, const char* description, const char* subscribe_payload) {

    std::signal(SIGINT, on_signal);  // Handle Ctrl+C

    std::cout << "=== Wirekrak Connection - Transport vs Delivery (" << name << ") ===\n\n"
        << description << "\n\n";

    // -------------------------------------------------------------------------
    // Connection setup
    // -------------------------------------------------------------------------
    wirekrak::core::transport::telemetry::Connection telemetry;
    wirekrak::core::transport::Connection<WS> connection(telemetry);

    connection.on_connect([&] {
        std::cout << "[example] Connected to " << name << " WebSocket\n";
    });

    connection.on_message([&](std::string_view msg) {
        std::cout << "[example] RX message (" << msg.size() << " bytes)\n";
        //std::cout << "[example] RX message (" << msg.size() << " bytes)\n --> " << msg << "\n";
    });

    connection.on_disconnect([&] {
        std::cout << "[example] Disconnected\n";
    });

    // ---------------------------------------------------------------------
    // Open connection
    // ---------------------------------------------------------------------
    std::cout << "[example] Connecting to " << url << "\n";
    if (connection.open(url) != wirekrak::core::transport::Error::None) {
        std::cerr << "[example] Failed to connect\n";
        return 1;
    }

    // ---------------------------------------------------------------------
    // Subscribe
    // ---------------------------------------------------------------------
    std::cout << "[example] Sending subscription\n";
    (void)connection.send(subscribe_payload);

    // -------------------------------------------------------------------------
    // Main polling loop (runs until Ctrl+C)
    // -------------------------------------------------------------------------
    while (running.load(std::memory_order_relaxed)) {
        connection.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // ---------------------------------------------------------------------
    // Close connection
    // ---------------------------------------------------------------------
    connection.close();

    // Drain any remaining messages (aprox. 200 ms)
    for (int i = 0; i < 20; ++i) {
        connection.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // ---------------------------------------------------------------------
    // Dump telemetry
    // ---------------------------------------------------------------------

    std::cout << "\n=== Connection Telemetry ===\n";
    telemetry.debug_dump(std::cout);

    std::cout << "\n=== WebSocket Telemetry ===\n";
    telemetry.websocket.debug_dump(std::cout);

    // ---------------------------------------------------------------------
    // How to read this
    // ---------------------------------------------------------------------

    std::cout << "\n=== How to read this ===\n"

              << "Traffic\n"
              << "  RX messages\n"
              << "    Number of complete messages delivered to the user.\n\n"

              << "Message shape\n"
              << "  RX message bytes\n"
              << "    Distribution of assembled message sizes observed\n"
              << "    by the transport.\n\n"

              << "  Fragments/msg\n"
              << "    How messages were framed on the wire.\n"
              << "    Values greater than 1 indicate multi-frame delivery.\n\n"

              << "Fragments total\n"
              << "  RX fragments\n"
              << "    Total number of fragment frames observed.\n"
              << "    Zero means all messages arrived as single frames.\n\n"

              << "Wirekrak reports observable wire reality -\n"
              << "not sender intent or application assumptions.\n";

    return 0;
}

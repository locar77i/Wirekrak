#pragma once

#include <cstdint>
#include <vector>
#include <memory>

#include "wirekrak/core/transport/telemetry/connection.hpp"
#include "wirekrak/core/transport/connection.hpp"
#include "wirekrak/core/transport/connection/signal.hpp"
#include "common/mock_websocket.hpp"

/*
===============================================================================
 Connection Test Harness
===============================================================================

Purpose:
--------
Provides a minimal, deterministic harness for testing
wirekrak::core::transport::Connection FSM behavior.

Design:
-------
- Telemetry outlives Connection
- Connection lifetime is explicit and controllable
- Connection signals are drained deterministically
- No callbacks, no threads, no hidden behavior

This enables:
- Destructor behavior testing
- Re-creation of Connection within a single test
- Precise lifecycle assertions

===============================================================================
*/

namespace wirekrak::core::transport::test {

struct ConnectionHarness {
    // -------------------------------------------------------------------------
    // Persistent telemetry (must outlive Connection)
    // -------------------------------------------------------------------------
    telemetry::Connection telemetry;

    // -------------------------------------------------------------------------
    // Connection under test (explicit lifetime)
    // -------------------------------------------------------------------------
    std::unique_ptr<Connection<MockWebSocket>> connection;

    // -------------------------------------------------------------------------
    // Event counters
    // -------------------------------------------------------------------------
    std::uint32_t connect_signals{0};
    std::uint32_t disconnect_signals{0};
    std::uint32_t retry_schedule_signals{0};
    std::uint32_t liveness_warning_signals{0};

    // Ordered signal log (optional inspection)
    std::vector<connection::Signal> signals;

    // -------------------------------------------------------------------------
    // Constructor
    // -------------------------------------------------------------------------
    ConnectionHarness(
        std::chrono::seconds heartbeat_timeout = HEARTBEAT_TIMEOUT,
        std::chrono::seconds message_timeout = MESSAGE_TIMEOUT,
        double liveness_warning_ratio = LIVENESS_WARNING_RATIO
    )
    {
        test::MockWebSocket::reset();
        make_connection(heartbeat_timeout,  message_timeout, liveness_warning_ratio);
    }

    // -------------------------------------------------------------------------
    // Create a fresh Connection instance
    // -------------------------------------------------------------------------
    inline void make_connection(
        std::chrono::seconds heartbeat_timeout = HEARTBEAT_TIMEOUT,
        std::chrono::seconds message_timeout = MESSAGE_TIMEOUT,
        double liveness_warning_ratio = LIVENESS_WARNING_RATIO
    )
    {
        connection = std::make_unique<Connection<MockWebSocket>>(telemetry, heartbeat_timeout, message_timeout, liveness_warning_ratio);
    }

    // -------------------------------------------------------------------------
    // Destroy the Connection (forces destructor behavior)
    // -------------------------------------------------------------------------
    inline void destroy_connection()
    {
        connection.reset(); // ~Connection() runs here
    }

    // -------------------------------------------------------------------------
    // Drain all pending connection signals
    // -------------------------------------------------------------------------
    inline void drain_signals() noexcept {
        if (!connection) {
            return;
        }

        connection::Signal sig;
        while (connection->poll_signal(sig)) {
            switch (sig) {
            case connection::Signal::Connected:
                ++connect_signals;
                break;

            case connection::Signal::Disconnected:
                ++disconnect_signals;
                break;

            case connection::Signal::RetryScheduled:
                ++retry_schedule_signals;
                break;
            
            case connection::Signal::LivenessThreatened:
                ++liveness_warning_signals;
                break;

            case connection::Signal::None:
            default:
                break;
            }

            signals.push_back(sig);
        }
    }

    // -------------------------------------------------------------------------
    // Reset counters and signal log (does NOT affect connection state)
    // -------------------------------------------------------------------------
    inline void reset_counters() noexcept {
        connect_signals = 0;
        disconnect_signals = 0;
        retry_schedule_signals = 0;
        liveness_warning_signals = 0;
        signals.clear();
    }
};

} // namespace wirekrak::core::transport::test

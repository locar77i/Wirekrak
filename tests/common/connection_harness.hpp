#pragma once

#include <cstdint>
#include <vector>
#include <memory>

#include "wirekrak/core/transport/telemetry/connection.hpp"
#include "wirekrak/core/transport/connection.hpp"
#include "wirekrak/core/transport/connection/transition_event.hpp"
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
- Transition events are drained deterministically
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
    std::uint32_t connect_events{0};
    std::uint32_t disconnect_events{0};
    std::uint32_t retry_schedule_events{0};
    std::uint32_t liveness_warning_events{0};

    // Ordered event log (optional inspection)
    std::vector<TransitionEvent> events;

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
    // Drain all pending transition events
    // -------------------------------------------------------------------------
    inline void drain_events() noexcept {
        if (!connection) {
            return;
        }

        TransitionEvent ev;
        while (connection->poll_event(ev)) {
            switch (ev) {
            case TransitionEvent::Connected:
                ++connect_events;
                break;

            case TransitionEvent::Disconnected:
                ++disconnect_events;
                break;

            case TransitionEvent::RetryScheduled:
                ++retry_schedule_events;
                break;
            
            case TransitionEvent::LivenessThreatened:
                ++liveness_warning_events;
                break;

            case TransitionEvent::None:
            default:
                break;
            }

            events.push_back(ev);
        }
    }

    // -------------------------------------------------------------------------
    // Reset counters and event log (does NOT affect connection state)
    // -------------------------------------------------------------------------
    inline void reset_counters() noexcept {
        connect_events = 0;
        disconnect_events = 0;
        retry_schedule_events = 0;
        liveness_warning_events = 0;
        events.clear();
    }
};

} // namespace wirekrak::core::transport::test

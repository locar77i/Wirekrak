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
#pragma once

#include <cstdint>
#include <vector>
#include <memory>

#include "wirekrak/core/transport/websocket_concept.hpp"
#include "wirekrak/core/transport/connection.hpp"
#include "wirekrak/core/transport/connection/signal.hpp"
#include "wirekrak/core/transport/telemetry/connection.hpp"
#include "wirekrak/core/policy/transport/connection_bundle.hpp"
#include "wirekrak/core/preset/control_ring_default.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"
#include "common/mock_websocket.hpp"
#include "common/test_check.hpp"

// -----------------------------------------------------------------------------
// Setup environment
// -----------------------------------------------------------------------------
using namespace wirekrak::core;
using namespace wirekrak::core::transport;

using MessageRingUnderTest = preset::DefaultMessageRing; // Golbal message ring buffer (transport → session)
using ControlRingUnderTest = preset::DefaultControlRing; // Golbal control ring buffer (transport → session)

using WebSocketUnderTest =
    test::MockWebSocket<
        ControlRingUnderTest, 
        MessageRingUnderTest
    >;

// Assert that WebSocketUnderTest conforms to transport::WebSocketConcept concept
static_assert(WebSocketConcept<WebSocketUnderTest>);

using ConnectionUnderTest =
    Connection<
        WebSocketUnderTest,
        MessageRingUnderTest
    >;

static MessageRingUnderTest g_ring;   // Golbal message ring buffer (transport → session)


namespace wirekrak::core::transport::test {
namespace harness {

template<
    WebSocketConcept WS   = WebSocketUnderTest,
    typename MessageRing  = MessageRingUnderTest,
    typename PolicyBundle = policy::transport::ConnectionDefault
>
struct Connection {
    // -------------------------------------------------------------------------
    // Persistent telemetry (must outlive Connection)
    // -------------------------------------------------------------------------
    telemetry::Connection telemetry;

    // -------------------------------------------------------------------------
    // Connection under test (explicit lifetime)
    // -------------------------------------------------------------------------
    using ConnectionType = wirekrak::core::transport::Connection<WS, MessageRing, PolicyBundle>;

    std::unique_ptr<ConnectionType> connection;

    // -------------------------------------------------------------------------
    // Event counters
    // -------------------------------------------------------------------------
    std::uint32_t connect_signals{0};
    std::uint32_t disconnect_signals{0};
    std::uint32_t retry_immediate_signals{0};
    std::uint32_t retry_schedule_signals{0};
    std::uint32_t liveness_warning_signals{0};

    // Ordered signal log (optional inspection)
    std::vector<connection::Signal> signals;

    // -------------------------------------------------------------------------
    // Constructor
    // -------------------------------------------------------------------------
    Connection()
    {
        WebSocketUnderTest::reset();
        make_connection();
    }

    // -------------------------------------------------------------------------
    // Create a fresh Connection instance
    // -------------------------------------------------------------------------
    inline void make_connection()
    {
        connection = std::make_unique<ConnectionType>(g_ring, telemetry);
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

            case connection::Signal::RetryImmediate:
                ++retry_immediate_signals;
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
        retry_immediate_signals = 0;
        retry_schedule_signals = 0;
        liveness_warning_signals = 0;
        signals.clear();
    }
};

} // namespace harness

using ConnectionHarness = harness::Connection<WebSocketUnderTest, MessageRingUnderTest>;

} // namespace wirekrak::core::transport::test

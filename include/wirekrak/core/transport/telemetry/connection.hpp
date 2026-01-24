#pragma once

#include <type_traits>

#include "wirekrak/core/transport/telemetry/websocket.hpp"

#include "lcr/metrics/atomic/counter.hpp"
#include "lcr/format.hpp"

namespace wirekrak::core::transport::telemetry {

// ============================================================================
// Connection Telemetry (v1 - frozen)
//
// Observes connection-level state transitions and decisions.
// Does NOT duplicate WebSocket telemetry.
// Mechanical facts only.
// ============================================================================

struct alignas(64) Connection final {
    // ---------------------------------------------------------------------
    // Lifecycle & state transitions
    // ---------------------------------------------------------------------

    // open() invoked by user
    lcr::metrics::atomic::counter32 open_calls_total;

    // Successfully reached State::Connected
    lcr::metrics::atomic::counter32 connect_success_total;

    // Failed initial connection attempt
    lcr::metrics::atomic::counter32 connect_failure_total;

    // Explicit close() invoked by user
    lcr::metrics::atomic::counter32 close_calls_total;

    // Transport closed while connected (any cause)
    lcr::metrics::atomic::counter32 disconnect_events_total;

    // ---------------------------------------------------------------------
    // Liveness decisions
    // ---------------------------------------------------------------------

    // Forced disconnect due to liveness timeout (heartbeat + message)
    lcr::metrics::atomic::counter32 liveness_timeouts_total;

    // ---------------------------------------------------------------------
    // Retry mechanics (decisions, not timing)
    // ---------------------------------------------------------------------

    // Entered State::WaitingReconnect
    lcr::metrics::atomic::counter32 retry_cycles_started_total;

    // Reconnect attempt initiated
    lcr::metrics::atomic::counter32 retry_attempts_total;

    // Reconnect succeeded
    lcr::metrics::atomic::counter32 retry_success_total;

    // Reconnect failed (attempted but did not connect)
    lcr::metrics::atomic::counter32 retry_failure_total;

    // ---------------------------------------------------------------------
    // Message handoff (WS → user boundary)
    // ---------------------------------------------------------------------

    // Messages forwarded to user callback
    lcr::metrics::atomic::counter64 messages_forwarded_total;

    // ---------------------------------------------------------------------
    // Send gating
    // ---------------------------------------------------------------------

    // send() called by user
    lcr::metrics::atomic::counter64 send_calls_total;

    // send() rejected due to non-connected state
    lcr::metrics::atomic::counter64 send_rejected_total;

    // ---------------------------------------------------------------------
    // Sub-telemetry
    // ---------------------------------------------------------------------

    transport::telemetry::WebSocket websocket;

    // ---------------------------------------------------------------------
    // Snapshot support
    // ---------------------------------------------------------------------

    inline void copy_to(Connection& other) const noexcept {
        open_calls_total.copy_to(other.open_calls_total);
        connect_success_total.copy_to(other.connect_success_total);
        connect_failure_total.copy_to(other.connect_failure_total);
        close_calls_total.copy_to(other.close_calls_total);
        disconnect_events_total.copy_to(other.disconnect_events_total);

        liveness_timeouts_total.copy_to(other.liveness_timeouts_total);

        retry_cycles_started_total.copy_to(other.retry_cycles_started_total);
        retry_attempts_total.copy_to(other.retry_attempts_total);
        retry_success_total.copy_to(other.retry_success_total);
        retry_failure_total.copy_to(other.retry_failure_total);

        messages_forwarded_total.copy_to(other.messages_forwarded_total);

        send_calls_total.copy_to(other.send_calls_total);
        send_rejected_total.copy_to(other.send_rejected_total);

        // Sub-telemetry
        websocket.copy_to(other.websocket);
    }

    inline void debug_dump(std::ostream& os) const noexcept {
        os << "\n=== Connection Telemetry ===\n";

        // ---------------------------------------------------------------------
        // Lifecycle & state transitions
        // ---------------------------------------------------------------------
        os << "Lifecycle\n";
        os << "  Open calls            : " << lcr::format_number_exact(open_calls_total.load()) << '\n';
        os << "  Connect success       : " << lcr::format_number_exact(connect_success_total.load()) << '\n';
        os << "  Connect failure       : " << lcr::format_number_exact(connect_failure_total.load()) << '\n';
        os << "  Close calls           : " << lcr::format_number_exact(close_calls_total.load()) << '\n';
        os << "  Disconnect events     : " << lcr::format_number_exact(disconnect_events_total.load()) << '\n';

        // ---------------------------------------------------------------------
        // Liveness decisions
        // ---------------------------------------------------------------------
        os << "\nLiveness\n";
        os << "  Liveness timeouts     : " << lcr::format_number_exact(liveness_timeouts_total.load()) << '\n';

        // ---------------------------------------------------------------------
        // Retry mechanics
        // ---------------------------------------------------------------------
        os << "\nRetry\n";
        os << "  Retry cycles started  : " << lcr::format_number_exact(retry_cycles_started_total.load()) << '\n';
        os << "  Retry attempts        : " << lcr::format_number_exact(retry_attempts_total.load()) << '\n';
        os << "  Retry success         : " << lcr::format_number_exact(retry_success_total.load()) << '\n';
        os << "  Retry failure         : " << lcr::format_number_exact(retry_failure_total.load()) << '\n';

        // ---------------------------------------------------------------------
        // Message handoff
        // ---------------------------------------------------------------------
        os << "\nMessage handoff\n";
        os << "  Messages forwarded   : " << lcr::format_number_exact(messages_forwarded_total.load()) << '\n';

        // ---------------------------------------------------------------------
        // Send gating
        // ---------------------------------------------------------------------
        os << "\nSend\n";
        os << "  Send calls            : " << lcr::format_number_exact(send_calls_total.load()) << '\n';
        os << "  Send rejected         : " << lcr::format_number_exact(send_rejected_total.load()) << '\n';
    }

};

// -------------------------------------------------------------------------
// Invariants
// -------------------------------------------------------------------------
static_assert(std::is_standard_layout_v<Connection>, "telemetry::Connection must be standard layout");
static_assert(std::is_trivially_destructible_v<Connection>, "telemetry::Connection must be trivially destructible");
static_assert(!std::is_polymorphic_v<Connection>, "telemetry::Connection must not be polymorphic");
static_assert(alignof(Connection) == 64, "telemetry::Connection must be cache-line aligned");

} // namespace wirekrak::core::transport::telemetry


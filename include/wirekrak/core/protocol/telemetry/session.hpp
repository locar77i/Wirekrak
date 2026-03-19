#pragma once

#include <type_traits>

#include "wirekrak/core/transport/telemetry/connection.hpp"
#include "lcr/metrics/counter.hpp"
#include "lcr/metrics/stats/size.hpp"
#include "lcr/metrics/stats/sampler.hpp"
#include "lcr/metrics/stats/duration.hpp"
#include "lcr/metrics/latency_histogram.hpp"
#include "lcr/format.hpp"


namespace wirekrak::core::protocol::telemetry {

// ============================================================================
// Session Telemetry (v1 - frozen)
//
// Observes protocol-layer mechanics shared by all exchange sessions.
//
// Design principles:
//   • exchange-agnostic
//   • mechanical facts only
//   • no clocks
//   • no policy
//   • no allocations
//   • atomic-only metrics
//
// Captures:
//   • protocol message processing
//   • parser outcomes
//   • protocol request emission
//   • rejection handling
//   • replay activity
//   • user backpressure events
//
// ============================================================================

struct alignas(64) Session final {

    // ---------------------------------------------------------------------
    // Message processing
    // ---------------------------------------------------------------------

    lcr::metrics::counter64 messages_processed_total;  // Total protocol messages processed
    lcr::metrics::stats::size32 messages_per_poll;     // Number of messages handled per poll() cycle

    // ---------------------------------------------------------------------
    // Parser outcomes
    // ---------------------------------------------------------------------

    lcr::metrics::counter64 parse_success_total;       // Successfully parsed and routed messages
    lcr::metrics::counter64 parse_failure_total;       // Parser failures (invalid JSON, schema mismatch, etc.)
    lcr::metrics::counter64 parse_backpressure_total;  // Parser rejected message due to backpressure

    // ---------------------------------------------------------------------
    // Session requests
    // ---------------------------------------------------------------------

    lcr::metrics::counter64 requests_emitted_total;           // Session requests emitted (subscribe, unsubscribe, ping, etc.)
    lcr::metrics::counter64 subscriptions_requested_total;    // Subscribe requests issued
    lcr::metrics::counter64 unsubscriptions_requested_total;  // Unsubscribe requests issued

    // ---------------------------------------------------------------------
    // Rejection notices
    // ---------------------------------------------------------------------

    lcr::metrics::counter64 rejection_notices_total;  // Rejection notices received from exchange

    // ---------------------------------------------------------------------
    // Replay activity
    // ---------------------------------------------------------------------
    
    lcr::metrics::counter64 replay_requests_total;  // Number of replay operations triggered after reconnect
    lcr::metrics::counter64 replay_symbols_total;   // Total symbols replayed during reconnect recovery

    // --------------------------------------------------------
    // Delivery failures
    // --------------------------------------------------------

    lcr::metrics::counter64 request_batching_failures_total;
    lcr::metrics::counter64 user_delivery_failures_total;  // Session backpressure caused by user not draining messages fast enough

    // ---------------------------------------------------------------------
    // Data-plane pressure
    // ---------------------------------------------------------------------

    lcr::metrics::stats::size16 message_ring_depth;  // Measures the actual load level of the message processing ring

    // ---------------------------------------------------------------------
    // Transport backpressure
    // ---------------------------------------------------------------------

    lcr::metrics::stats::sampler32 transport_overload_streak;     // Allows to measure how severe is the transport backpressure escalation
    
    // ---------------------------------------------------------------------
    // User backpressure
    // ---------------------------------------------------------------------
    
    lcr::metrics::stats::sampler32 user_overload_streak;     // Allows to measure how severe is the user backpressure escalation

    // ---------------------------------------------------------------------
    // Timing
    // ---------------------------------------------------------------------

    lcr::metrics::stats::duration64 poll_duration;            // Measures the duration of each poll() cycle 
    lcr::metrics::latency_histogram handoff_latency;          // Latency of message handoff between transport and protocol layers
    lcr::metrics::stats::duration64 message_process_duration; // Measures the duration of every message processing (time spent inside the protocol layer to process one message)

    // ---------------------------------------------------------------------
    // Sub-telemetry
    // ---------------------------------------------------------------------

    transport::telemetry::Connection connection;

    // ---------------------------------------------------------------------
    // Snapshot support
    // ---------------------------------------------------------------------

    inline void copy_to(Session& other) const noexcept {

        // Message processing
        messages_processed_total.copy_to(other.messages_processed_total);
        messages_per_poll.copy_to(other.messages_per_poll);

        // Parser outcomes
        parse_success_total.copy_to(other.parse_success_total);
        parse_failure_total.copy_to(other.parse_failure_total);
        parse_backpressure_total.copy_to(other.parse_backpressure_total);

        // Session requests
        requests_emitted_total.copy_to(other.requests_emitted_total);
        subscriptions_requested_total.copy_to(other.subscriptions_requested_total);
        unsubscriptions_requested_total.copy_to(other.unsubscriptions_requested_total);

        // Rejection notices
        rejection_notices_total.copy_to(other.rejection_notices_total);

        // Replay activity
        replay_requests_total.copy_to(other.replay_requests_total);
        replay_symbols_total.copy_to(other.replay_symbols_total);

        // Delivery failures
        request_batching_failures_total.copy_to(other.request_batching_failures_total);
        user_delivery_failures_total.copy_to(other.user_delivery_failures_total);

        // Data-plane pressure
        message_ring_depth.copy_to(other.message_ring_depth);

        // Transport backpressure
        transport_overload_streak.copy_to(other.transport_overload_streak);

        // User backpressure
        user_overload_streak.copy_to(other.user_overload_streak);

        // Timing
        poll_duration.copy_to(other.poll_duration);
        handoff_latency.copy_to(other.handoff_latency);
        message_process_duration.copy_to(other.message_process_duration);

        // ---------------------------------------------------------------------
        // Sub-telemetry
        // ---------------------------------------------------------------------
        connection.copy_to(other.connection);
    }

    // ---------------------------------------------------------------------
    // Debug dump
    // ---------------------------------------------------------------------

    inline void debug_dump(std::ostream& os) const noexcept {

        os << "\n=== Session Telemetry ===\n";

        // Message processing
        os << "Message processing\n";
        os << "  Messages total     : " << lcr::format_number_exact(messages_processed_total.load()) << '\n';
        os << "  Messages per poll  : "; messages_per_poll.dump(os); os << '\n';

        // Parser outcomes
        os << "\nParser\n";
        os << "  Parse success      : " << lcr::format_number_exact(parse_success_total.load()) << '\n';
        os << "  Parse failure      : " << lcr::format_number_exact(parse_failure_total.load()) << '\n';
        os << "  Parse backpressure : " << lcr::format_number_exact(parse_backpressure_total.load()) << '\n';

        // Session requests
        os << "\nRequests\n";
        os << "  Requests emitted   : " << lcr::format_number_exact(requests_emitted_total.load()) << '\n';
        os << "  Subscriptions      : " << lcr::format_number_exact(subscriptions_requested_total.load()) << '\n';
        os << "  Unsubscriptions    : " << lcr::format_number_exact(unsubscriptions_requested_total.load()) << '\n';

        // Rejections
        os << "\nRejections\n";
        os << "  Notices received   : " << lcr::format_number_exact(rejection_notices_total.load()) << '\n';

        // Replay
        os << "\nReplay\n";
        os << "  Replay requests    : "  << lcr::format_number_exact(replay_requests_total.load()) << '\n';
        os << "  Replay symbols     : " << lcr::format_number_exact(replay_symbols_total.load()) << '\n';

        // Delivery failures
        os << "\nDelivery failures\n";
        os << "  Request batching   : " << lcr::format_number_exact(request_batching_failures_total.load()) << '\n';
        os << "  User delivery      : " << lcr::format_number_exact(user_delivery_failures_total.load()) << '\n';

        // Data-plane pressure
        os << "\nData-plane pressure\n";
        os << "  Message ring depth : "; message_ring_depth.dump(os); os << '\n';

        // Transport backpressure
        os << "\nTransport backpressure\n";
        os << "  Overload streak    : "; transport_overload_streak.dump(os); os << '\n';

        // User backpressure
        os << "\nUser backpressure\n";
        os << "  Overload streak    : "; user_overload_streak.dump(os); os << '\n';

        os << "\nTiming\n";
        os << "  Poll duration      : "; poll_duration.dump(os); os << '\n';
        os << "  Message handoff    : "; handoff_latency.dump(os); os << '\n';
        os << "  Process message    : "; message_process_duration.dump(os); os << '\n';

        // ---------------------------------------------------------------------
        // Sub-telemetry
        // ---------------------------------------------------------------------
        connection.debug_dump(os);
    }
};

// -------------------------------------------------------------------------
// Invariants
// -------------------------------------------------------------------------

static_assert(std::is_standard_layout_v<Session>, "telemetry::Session must be standard layout");
static_assert(std::is_trivially_destructible_v<Session>, "telemetry::Session must be trivially destructible");
static_assert(!std::is_polymorphic_v<Session>, "telemetry::Session must not be polymorphic");
static_assert(alignof(Session) == 64, "telemetry::Session must be cache-line aligned");

} // namespace wirekrak::core::protocol::telemetry

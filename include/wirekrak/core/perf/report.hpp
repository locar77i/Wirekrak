#pragma once

#include <ostream>
#include <iomanip>

#include "wirekrak/core/protocol/telemetry/session.hpp"
#include "lcr/format.hpp"

namespace wirekrak::core::perf {

// =====================================================================================
// Wirekrak Performance Report (Citadel-style)
// =====================================================================================
// - Read-only view over telemetry::Session
// - Zero allocations
// - Deterministic formatting
// =====================================================================================

class Report {
public:
    explicit Report(const core::protocol::telemetry::Session& t) noexcept
        : t_(t)
        , total_bytes_     ( t.connection.websocket.bytes_rx_total.load() )
        , total_msgs_      ( t.messages_per_poll.total() )
        , ingress_rate_    ( t.connection.websocket.ws_message_ingress_duration.rate_per_sec() )
        , process_rate_    ( t.message_process_duration.rate_per_sec() )
        , poll_rate_       ( t.poll_duration.rate_per_sec() )
        , headroom_        ( ingress_rate_ > 0 ? process_rate_ / ingress_rate_ : 0.0 )
        , latency_         ( t.end_to_end_latency.compute_percentiles() )
        , healthy_ns_      ( t.healthy_time_ns.load() )
        , backpressure_ns_ ( t.backpressure_time_ns.load() )
        , total_ns_        ( healthy_ns_ + backpressure_ns_ )
        , efficiency_      ( total_ns_ > 0 ? (double)healthy_ns_ / (double)total_ns_ : 1.0 )
        , avg_queue_depth_ ( t.message_ring_depth.avg() )
        , avg_latency_     ( ingress_rate_ > 0 ? avg_queue_depth_ / ingress_rate_ * 1'000'000'000 : 0.0 )
        , utilization_     ( process_rate_ > 0 ? ingress_rate_ / process_rate_ : 0.0 )
        {}

    inline void dump(std::ostream& os) const noexcept {
        header_(os);
        executive_summary_(os);
        latency_analysis_(os);
        throughput_analysis_(os);
        health_analysis_(os);
        transport_analysis_(os);
        memory_analysis_(os);
        message_shape_analysis_(os);
        control_plane_analysis_(os);
        footer_(os);
    }

private:
    const core::protocol::telemetry::Session& t_;

    const std::uint64_t total_bytes_;
    const std::uint64_t total_msgs_;
    const double ingress_rate_;
    const double process_rate_;
    const double poll_rate_;
    const double headroom_;

    const lcr::metrics::latency_percentiles latency_;

    const std::uint64_t healthy_ns_;
    const std::uint64_t backpressure_ns_;
    const std::uint64_t total_ns_;
    const double efficiency_;
    const double avg_queue_depth_;
    const double avg_latency_;
    const double utilization_;

private:

    // =============================================================================
    // HEADER
    // =============================================================================
    inline void header_(std::ostream& os) const noexcept {
        os << "================================================================================\n";
        os << "                           WIREKRAK PERFORMANCE REPORT\n";
        os << "================================================================================\n";
    }

    inline void print_utilization_observation_(std::ostream& os, double utilization) const noexcept {
        if (utilization < 0.5) {
            os << "  - System operating far below saturation (" << utilization * 100.0 << "%)\n";
        }
        else if (utilization < 0.8) {
            os << "  - System operating in safe utilization range (" << utilization * 100.0 << "%)\n";
        }
        else if (utilization < 0.95) {
            os << "  - System approaching saturation (" << utilization * 100.0 << "%)\n";
        }
        else {
            os << "  - System near saturation → risk of queue explosion (" << utilization * 100.0 << "%)\n";
        }
    }

    // =============================================================================
    // EXECUTIVE SUMMARY
    // =============================================================================
    inline void executive_summary_(std::ostream& os) const noexcept {
        os << "\n[1] EXECUTIVE SUMMARY\n";
        os << "--------------------------------------------------------------------------------\n";

        os << "\nThroughput\n";
        os << "  Data volume      : " << lcr::format_bytes(total_bytes_) << '\n';
        os << "  Total messages   : " << lcr::format_number_exact(total_msgs_) << "\n\n";
        os << "  Ingress rate     : " << lcr::format_throughput(ingress_rate_, "msg/s") << '\n';
        os << "  Process rate     : " << lcr::format_throughput(process_rate_, "msg/s") << " (x" << lcr::format_number_exact(headroom_) << " headroom)\n";
        os << "  Poll rate        : " << lcr::format_throughput(poll_rate_, "polls/s") << '\n';

        print_latency_block_(os, "Latency (end-to-end)", latency_);

        os << "\nStability\n";
        os << "  Efficiency       : " << std::fixed << std::setprecision(6) << (efficiency_ * 100.0 - 0.000001) << " % (" << efficiency_label_(efficiency_) << ")\n";
        os << "  Backpressure     : " << lcr::format_number_exact( t_.connection.websocket.backpressure_detected_total.load() ) << " (events)\n";
        os << "  Ingress retries  : " << lcr::format_number_exact( t_.connection.websocket.message_ring_failures_total.load() ) << " (no message drops)\n";
        os << "  Errors           : " << lcr::format_number_exact( t_.connection.websocket.rx_errors_total.load() ) << '\n';

        os << "\nQueueing model (Little's Law)\n";
        os << "  Arrival rate     : " << lcr::format_throughput(ingress_rate_, "msg/s") << '\n';
        os << "  Service rate     : " << lcr::format_throughput(process_rate_, "msg/s") << '\n';
        os << "  Utilization      : " << std::fixed << std::setprecision(2) << (utilization_ * 100.0) << " %\n";
        os << "\n";
        os << "  Avg queue depth  : " << std::fixed << std::setprecision(2) << avg_queue_depth_ << '\n';
        os << "  Avg latency      : " << lcr::format_duration( avg_latency_ ) << '\n';

        os << "\nObservations\n";
        print_utilization_observation_(os, utilization_);
        //os << "  - Queue remains stable and bounded\n";
        //os << "  - No risk of latency explosion under current load\n";

        os << "\nSystem behavior\n";
        os << "  Reconnects       : " << lcr::format_number_exact(t_.connection.retry_success_total.load()) << '\n';
        os << "  Replay events    : " << lcr::format_number_exact(t_.replay_requests_total.load()) << '\n';
    }

    // =============================================================================
    // LATENCY
    // =============================================================================
    inline void latency_analysis_(std::ostream& os) const noexcept {
        os << "\n\n[2] LATENCY ANALYSIS\n";
        os << "--------------------------------------------------------------------------------\n";

        print_latency_block_(os, "Latency (end-to-end)", latency_, false);

        os << "\nIngress (transport -> ring)\n";
        os << "  Avg             : " << lcr::format_duration(t_.connection.websocket.ws_message_ingress_duration.avg_ns()) << '\n';
        os << "  Min             : " << lcr::format_duration(t_.connection.websocket.ws_message_ingress_duration.min_ns()) << '\n';
        os << "  Max             : " << lcr::format_duration(t_.connection.websocket.ws_message_ingress_duration.max_ns()) << '\n';

        os << "\nProcessing (protocol layer)\n";
        os << "  Avg             : " << lcr::format_duration(t_.message_process_duration.avg_ns()) << '\n';
        os << "  Min             : " << lcr::format_duration(t_.message_process_duration.min_ns()) << '\n';
        os << "  Max             : " << lcr::format_duration(t_.message_process_duration.max_ns()) << '\n';
    }

    // =============================================================================
    // THROUGHPUT
    // =============================================================================
    inline void throughput_analysis_(std::ostream& os) const noexcept {
        os << "\n\n[3] THROUGHPUT & FLOW\n";
        os << "--------------------------------------------------------------------------------\n";

        os << "\nMessage Processing\n";
        os << "  Total            : " << lcr::format_number_exact(t_.messages_per_poll.total()) << '\n';
        os << "  Per poll (avg)   : " << std::fixed << std::setprecision(2) << t_.messages_per_poll.avg() << " msg/poll\n";
        os << "  Per poll (max)   : " << std::fixed << std::setprecision(2) << t_.messages_per_poll.max() << " msg/poll\n";

        os << "\nRates\n";
        os << "  Processing rate  : " << lcr::format_throughput(t_.message_process_duration.rate_per_sec(), "msg/s") << '\n';
        os << "  Polling rate     : " << lcr::format_throughput(t_.poll_duration.rate_per_sec(), "polls/s") << '\n';

        os << "\nParser\n";
        os << "  Success          : " << lcr::format_number_exact(t_.parse_success_total.load()) << '\n';
        os << "  Ignored          : " << lcr::format_number_exact(t_.parse_ignored_total.load()) << '\n';
        os << "  Failures         : " << lcr::format_number_exact(t_.parse_failure_total.load()) << '\n';
        os << "  Backpressure     : " << lcr::format_number_exact(t_.parse_backpressure_total.load()) << '\n';
    }

    // =============================================================================
    // HEALTH
    // =============================================================================
    inline void health_analysis_(std::ostream& os) const noexcept {
        os << "\n\n[4] SYSTEM HEALTH & STABILITY\n";
        os << "--------------------------------------------------------------------------------\n";

        os << "Lifecycle\n";
        os << "  Total time        : " << lcr::format_duration(total_ns_) << '\n';
        os << "  Healthy time      : " << lcr::format_duration(healthy_ns_) << '\n';
        os << "  Backpressure time : " << lcr::format_duration(backpressure_ns_) << '\n';
        os << "  Efficiency        : " << std::fixed << std::setprecision(6) << (efficiency_ * 100.0 - 0.000001) << " % (" << efficiency_label_(efficiency_) << ")\n";
        os << "  Degradation       : " << std::fixed << std::setprecision(6) << (100.0 - efficiency_ * 100.0 + 0.000001) << " %\n";

        os << "\nBackpressure\n";
        os << "  Events detected   : " << lcr::format_number_exact( t_.connection.websocket.backpressure_detected_total.load() ) << '\n';
        os << "  Events cleared    : " << lcr::format_number_exact( t_.connection.websocket.backpressure_cleared_total.load() ) << '\n';
        os << "  Overload streak   : " << lcr::format_number_exact( t_.transport_overload_streak.total() ) << '\n';

        os << "\nQueue Pressure\n";
        os << "  Message ring depth: "; t_.message_ring_depth.dump(os); os << '\n';
        os << "  Control ring depth: "; t_.control_ring_depth.dump(os); os << '\n';

        os << "\nDelivery\n";
        os << "  User failures     : " << lcr::format_number_exact(t_.user_delivery_failures_total.load()) << '\n';
    }

    // =============================================================================
    // TRANSPORT
    // =============================================================================
    inline void transport_analysis_(std::ostream& os) const noexcept {
        os << "\n\n[5] TRANSPORT & CONNECTION\n";
        os << "--------------------------------------------------------------------------------\n";

        const auto& c = t_.connection;

        os << "\nUser activity\n";
        os << "  Open calls        : " << lcr::format_number_exact(c.open_calls_total.load()) << '\n';
        os << "  Close calls       : " << lcr::format_number_exact(c.close_calls_total.load()) << '\n';

        os << "\nConnection\n";
        os << "  Connect success   : " << lcr::format_number_exact(c.connect_success_total.load()) << '\n';
        os << "  Disconnect events : " << lcr::format_number_exact(c.disconnect_events_total.load()) << '\n';
        os << "  Retry attempts    : " << lcr::format_number_exact(c.retry_attempts_total.load()) << '\n';
        os << "  Epoch transitions : " << lcr::format_number_exact(c.epoch_transitions_total.load()) << '\n';

        os << "\nLiveness\n";
        os << "  Timeouts          : " << lcr::format_number_exact(c.liveness_timeouts_total.load()) << '\n';
        os << "  Threats           : " << lcr::format_number_exact(c.signals_liveness_threatened_total.load()) << '\n';

        const auto& ws = c.websocket;

        os << "\nTraffic\n";
        os << "  RX bytes          : " << lcr::format_bytes(ws.bytes_rx_total.load()) << '\n';
        os << "  TX bytes          : " << lcr::format_bytes(ws.bytes_tx_total.load()) << '\n';
        os << "  RX messages       : " << lcr::format_number_exact(ws.messages_rx_total.load()) << '\n';
        os << "  TX messages       : " << lcr::format_number_exact(ws.messages_tx_total.load()) << '\n';

        os << "\nErrors\n";
        os << "  RX errors         : " << lcr::format_number_exact(ws.rx_errors_total.load()) << '\n';
        os << "  Send errors       : " << lcr::format_number_exact(ws.send_errors_total.load()) << '\n';
    }

    // =============================================================================
    // MEMORY
    // =============================================================================
    inline void memory_analysis_(std::ostream& os) const noexcept {
        os << "\n\n[6] MEMORY\n";
        os << "--------------------------------------------------------------------------------\n";

        const auto& ws = t_.connection.websocket;

        os << "\nMemory Behavior\n";
        os << "  Slot promotions   : " << lcr::format_number_exact(ws.slot_promotions_total.load()) << '\n';
        os << "  Pool depth        : "; ws.memory_pool_depth.dump(os); os << '\n';
    }

    // =============================================================================
    // MESSAGE SHAPE
    // =============================================================================
    inline void message_shape_analysis_(std::ostream& os) const noexcept {
        os << "\n\n[7] MESSAGE SHAPE\n";
        os << "--------------------------------------------------------------------------------\n";

        const auto& ws = t_.connection.websocket;

        os << "\nMessage size\n";
        os << "  Avg               : "  << lcr::format_bytes(ws.rx_message_bytes.avg()) << '\n';
        os << "  Min               : "  << lcr::format_bytes(ws.rx_message_bytes.min()) << '\n';
        os << "  Max               : "  << lcr::format_bytes(ws.rx_message_bytes.max()) << '\n';

        os << "\nFragmentation\n";
        os << "  RX fragments      : " << lcr::format_number_exact(ws.rx_fragments_total.load()) << '\n';
        os << "  Avg fragments/msg : " << std::setprecision(2) << ws.fragments_per_message.avg() << '\n';
        os << "  Max fragments/msg : " << lcr::format_number_exact(ws.fragments_per_message.max()) << '\n';
    }

    // =============================================================================
    // CONTROL PLANE
    // =============================================================================
    inline void control_plane_analysis_(std::ostream& os) const noexcept {
        os << "\n\n[8] CONTROL PLANE\n";
        os << "--------------------------------------------------------------------------------\n";

        os << "\nRequests\n";
        os << "  Emitted           : " << lcr::format_number_exact(t_.requests_emitted_total.load()) << '\n';
        os << "  Subscriptions     : " << lcr::format_number_exact(t_.subscriptions_requested_total.load()) << '\n';
        os << "  Unsubscriptions   : " << lcr::format_number_exact(t_.unsubscriptions_requested_total.load()) << '\n';

        os << "\nReplay\n";
        os << "  Replay requests   : " << lcr::format_number_exact(t_.replay_requests_total.load()) << '\n';
        os << "  Replay symbols    : " << lcr::format_number_exact(t_.replay_symbols_total.load()) << '\n';
    
        const auto& c = t_.connection;

        os << "\nConnection signals\n";
        os << "  Emitted           : " << lcr::format_number_exact(c.signals_emitted_total.load()) << '\n';
        os << "  Liveness threats  : " << lcr::format_number_exact(c.signals_liveness_threatened_total.load()) << '\n';
        os << "  Immediate retries : " << lcr::format_number_exact(c.signals_retry_immediate_total.load()) << '\n';
        os << "  Scheduled retries : " << lcr::format_number_exact(c.signals_retry_scheduled_total.load()) << '\n';

        os << "\nControl ring failures (send gating)\n";
        os << "  Total            : " << lcr::format_number_exact(c.control_ring_failures_total.load()) << '\n';
    }

    // =============================================================================
    // FOOTER
    // =============================================================================
    inline void footer_(std::ostream& os) const noexcept {
        os << "\n================================================================================\n";
    }

    // =============================================================================
    // HELPERS
    // =============================================================================
    static inline const char* efficiency_label_(double e) noexcept {
        if (e >= 0.999999) return "Perfect";
        if (e >= 0.99999)  return "Excellent";
        if (e >= 0.9999)   return "Optimal";
        if (e >= 0.999)    return "Very Good";
        if (e >= 0.99)     return "Good";
        if (e >= 0.95)     return "Suboptimal";
        if (e >= 0.80)     return "Degraded";
        return "Critical";
    }



    inline void print_latency_block_(std::ostream& os, const char* title, const lcr::metrics::latency_percentiles& h, bool tail_amplification = true) const noexcept {

        os << "\n" << title << "\n";

        // Pretty (human-oriented)
        os << "  p50 (median)     : " << lcr::format_duration(h.p50) << '\n';
        os << "  p90              : " << lcr::format_duration(h.p90) << '\n';
        os << "  p99              : " << lcr::format_duration(h.p99) << '\n';
        os << "  p99.9            : " << lcr::format_duration(h.p999) << '\n';
        os << "  p99.99           : " << lcr::format_duration(h.p9999) << '\n';
        os << "  p99.999          : " << lcr::format_duration(h.p99999) << '\n';
        os << "  p99.9999         : " << lcr::format_duration(h.p999999) << '\n';

        // tail amplification
        if (tail_amplification && h.p50 > 0) {
            const double amp_99     = (double)h.p99    / (double)h.p50;
            const double amp_9999   = (double)h.p9999  / (double)h.p50;
            const double amp_999999 = (double)h.p999999  / (double)h.p50;

            os << "\n";
            os << "  Tail amplification\n";
            os << "    p99 / p50      : x" << std::fixed << std::setprecision(2) << amp_99 << '\n';
            os << "    p99.99 / p50   : x" << std::fixed << std::setprecision(2) << amp_9999 << '\n';
            os << "    p99.9999 / p50 : x" << std::fixed << std::setprecision(2) << amp_999999 << '\n';
        }
    }
};

} // namespace wirekrak::core::perf

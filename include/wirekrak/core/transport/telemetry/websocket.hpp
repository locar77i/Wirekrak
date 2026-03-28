#pragma once

#include <type_traits>

#include "lcr/metrics/atomic/counter.hpp"
#include "lcr/metrics/atomic/stats/size.hpp"
#include "lcr/metrics/atomic/stats/sampler.hpp"
#include "lcr/metrics/atomic/stats/duration.hpp"
#include "lcr/format.hpp"

namespace wirekrak::core::transport::telemetry {

// ============================================================================
// WebSocket Telemetry (v1 - frozen)
//
// Transport-level observability contract shared by all WebSocket backends.
// Captures ONLY mechanical socket behavior.
//
// Design principles:
//   • no clocks
//   • no rates
//   • no policy
//   • no allocation
//   • no backend assumptions
//
// Throughput is derived exclusively via snapshot deltas.
// ============================================================================

struct alignas(64) WebSocket final {
    // ---------------------------------------------------------------------
    // Throughput (cumulative, monotonic)
    // ---------------------------------------------------------------------

    lcr::metrics::atomic::counter64 bytes_rx_total;
    lcr::metrics::atomic::counter64 bytes_tx_total;

    lcr::metrics::atomic::counter64 messages_rx_total;
    lcr::metrics::atomic::counter64 messages_tx_total;

    // --------------------------------------------------------
    // API activity
    // --------------------------------------------------------
    lcr::metrics::atomic::counter64 receive_calls_total;

    // ---------------------------------------------------------------------
    // Errors
    // ---------------------------------------------------------------------

    lcr::metrics::atomic::counter32 rx_errors_total;
    lcr::metrics::atomic::counter32 send_errors_total;

    // ---------------------------------------------------------------------
    // Lifecycle
    // ---------------------------------------------------------------------

    lcr::metrics::atomic::counter32 connect_events_total;
    lcr::metrics::atomic::counter32 close_events_total;

    // --------------------------------------------------------
    // Message shape
    // --------------------------------------------------------

    lcr::metrics::atomic::stats::size32 rx_message_bytes;  // Size of the currently assembled message being received

    // ---------------------------------------------------------------------
    // Fragmentation
    // ---------------------------------------------------------------------
    
    lcr::metrics::atomic::counter64 rx_fragments_total;  // Total number of WebSocket fragment frames observed on the wire
    lcr::metrics::atomic::stats::sampler32 fragments_per_message;  // Number of fragments per assembled message

    // --------------------------------------------------------
    // Access failures
    // --------------------------------------------------------

    lcr::metrics::atomic::counter64 message_ring_failures_total;
    lcr::metrics::atomic::counter64 memory_pool_failures_total;
    lcr::metrics::atomic::counter64 control_ring_failures_total;

    // --------------------------------------------------------
    // Backpressure events
    // --------------------------------------------------------

    lcr::metrics::atomic::counter32 backpressure_detected_total;
    lcr::metrics::atomic::counter32 backpressure_cleared_total;

    // --------------------------------------------------------
    // Memory behavior
    // --------------------------------------------------------

    lcr::metrics::atomic::counter64 slot_promotions_total;
    lcr::metrics::atomic::counter64 external_buffers_total;

    // ---------------------------------------------------------------------
    // Data-plane pressure
    // ---------------------------------------------------------------------

    lcr::metrics::atomic::stats::size16 memory_pool_depth;   // Measures the actual load level of the internal memory pool

    // --------------------------------------------------------
    // Control plane events
    // --------------------------------------------------------

    lcr::metrics::atomic::counter64 events_emitted_total; // Number of control plane events emitted by the transport (e.g. close, error, backpressure)

    // ---------------------------------------------------------------------
    // Timing
    // ---------------------------------------------------------------------

    lcr::metrics::atomic::stats::duration64 ws_message_ingress_duration; // Measures the processing duration of every message (including network + fragmentation)
    
    // ---------------------------------------------------------------------
    // Snapshot support
    // ---------------------------------------------------------------------

    inline void copy_to(WebSocket& other) const noexcept {
        // Traffic
        bytes_rx_total.copy_to(other.bytes_rx_total);
        bytes_tx_total.copy_to(other.bytes_tx_total);
        messages_rx_total.copy_to(other.messages_rx_total);
        messages_tx_total.copy_to(other.messages_tx_total);

        // API activity
        receive_calls_total.copy_to(other.receive_calls_total);

        // Errors
        rx_errors_total.copy_to(other.rx_errors_total);
        send_errors_total.copy_to(other.send_errors_total);

        // Lifecycle
        connect_events_total.copy_to(other.connect_events_total);
        close_events_total.copy_to(other.close_events_total);

        // Message shape
        rx_message_bytes.copy_to(other.rx_message_bytes);

        // Fragmentation
        rx_fragments_total.copy_to(other.rx_fragments_total);
        fragments_per_message.copy_to(other.fragments_per_message);

        // Access failures
        message_ring_failures_total.copy_to(other.message_ring_failures_total);
        memory_pool_failures_total.copy_to(other.memory_pool_failures_total);
        control_ring_failures_total.copy_to(other.control_ring_failures_total);

        // Backpressure events
        backpressure_detected_total.copy_to(other.backpressure_detected_total);
        backpressure_cleared_total.copy_to(other.backpressure_cleared_total);
    
        // Memory behavior
        slot_promotions_total.copy_to(other.slot_promotions_total);
        external_buffers_total.copy_to(other.external_buffers_total);

        // Data-plane pressure
        memory_pool_depth.copy_to(other.memory_pool_depth);

        // Control plane events
        events_emitted_total.copy_to(other.events_emitted_total);

        // Timing
        ws_message_ingress_duration.copy_to(other.ws_message_ingress_duration);
    }


    inline void debug_dump(std::ostream& os) const noexcept {

        os << "\n=== WebSocket Telemetry ===\n";

        // Traffic
        const uint64_t rx_msgs = messages_rx_total.load();
        os << "Traffic\n";
        os << "  RX bytes         : " << lcr::format_bytes(bytes_rx_total.load()) << '\n';
        os << "  TX bytes         : " << lcr::format_bytes(bytes_tx_total.load()) << '\n';
        os << "  RX messages      : " << lcr::format_number_exact(rx_msgs) << '\n';
        os << "  TX messages      : " << lcr::format_number_exact(messages_tx_total.load()) << '\n';

        // API activity
        os << "\nAPI Activity\n";
        os << "  Receive calls    : " << lcr::format_number_exact(receive_calls_total.load()) << '\n';

        // Errors
        os << "\nErrors\n";
        os << "  RX errors        : " << lcr::format_number_exact(rx_errors_total.load()) << '\n';
        os << "  Send errors      : " << lcr::format_number_exact(send_errors_total.load()) << '\n';

        // Lifecycle
        os << "\nLifecycle\n";
        os << "  Connect events   : " << lcr::format_number_exact(connect_events_total.load()) << '\n';
        os << "  Close events     : " << lcr::format_number_exact(close_events_total.load()) << '\n';

        // Message shape
        os << "\nMessage shape\n";
        os << "  RX message bytes : "; rx_message_bytes.dump(os); os << '\n';

        // Fragmentation
        os << "\nFragments total\n";
        os << "  RX fragments     : " << lcr::format_number_exact(rx_fragments_total.load()) << '\n';
        os << "  Fragments/msg    : "; fragments_per_message.dump(os); os << '\n';

        // Access failures
        os << "\nAccess failures\n";
        os << "  Message ring     : " << lcr::format_number_exact(message_ring_failures_total.load()) << '\n';
        os << "  Memory pool      : " << lcr::format_number_exact(memory_pool_failures_total.load()) << '\n';
        os << "  Control ring     : " << lcr::format_number_exact(control_ring_failures_total.load()) << '\n';

        // Backpressure events
        os << "\nBackpressure events\n";
        os << "  Detected         : " << lcr::format_number_exact(backpressure_detected_total.load()) << '\n';
        os << "  Cleared          : " << lcr::format_number_exact(backpressure_cleared_total.load()) << '\n';

        // Memory behavior
        os << "\nMemory behavior\n";
        os << "  Slot promotions  : " << lcr::format_number_exact(slot_promotions_total.load()) << '\n';
        os << "  External buffers : " << lcr::format_number_exact(external_buffers_total.load()) << '\n';

        // Data-plane pressure
        os << "\nData-plane pressure\n";
        os << "  Memory pool depth: "; memory_pool_depth.dump(os); os << '\n';

        // Control plane events
        os << "\nControl plane events\n";
        os << "  Events emitted   : " << lcr::format_number_exact(events_emitted_total.load()) << '\n';

        // Timing
        os << "\nTiming\n";
        os << "  Message ingress  : "; ws_message_ingress_duration.dump(os); os << '\n';
    }
};

// -------------------------------------------------------------------------
// Invariants (safe, non-fragile)
// -------------------------------------------------------------------------
static_assert(std::is_standard_layout_v<WebSocket>, "telemetry::WebSocket must be standard layout");
static_assert(std::is_trivially_destructible_v<WebSocket>, "telemetry::WebSocket must be trivially destructible");
static_assert(!std::is_polymorphic_v<WebSocket>, "telemetry::WebSocket must not be polymorphic");
static_assert(alignof(WebSocket) == 64, "telemetry::WebSocket must be cache-line aligned");

} // namespace wirekrak::core::transport::telemetry

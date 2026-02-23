#pragma once

#include <type_traits>

#include "lcr/metrics/atomic/counter.hpp"
#include "lcr/metrics/atomic/stats/size.hpp"
#include "lcr/metrics/atomic/stats/sampler.hpp"
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

    // ---------------------------------------------------------------------
    // Errors & lifecycle
    // ---------------------------------------------------------------------

    lcr::metrics::atomic::counter32 receive_errors_total;
    lcr::metrics::atomic::counter32 close_events_total;

    // ---------------------------------------------------------------------
    // Pressure / backlog
    // ---------------------------------------------------------------------

    // Size of the currently assembled message being received
    lcr::metrics::atomic::stats::size32 rx_message_bytes;

    // ---------------------------------------------------------------------
    // Shape / cost (per-event observations)
    // ---------------------------------------------------------------------

    // Number of fragments per assembled message
    lcr::metrics::atomic::stats::sampler32 fragments_per_message;

    // ---------------------------------------------------------------------
    // Received fragments
    // ---------------------------------------------------------------------
    // Total number of WebSocket fragment frames observed on the wire
    lcr::metrics::atomic::counter64 rx_fragments_total;

    // ---------------------------------------------------------------------
    // Snapshot support
    // ---------------------------------------------------------------------

    inline void copy_to(WebSocket& other) const noexcept {
        bytes_rx_total.copy_to(other.bytes_rx_total);
        bytes_tx_total.copy_to(other.bytes_tx_total);
        messages_rx_total.copy_to(other.messages_rx_total);
        messages_tx_total.copy_to(other.messages_tx_total);

        receive_errors_total.copy_to(other.receive_errors_total);
        close_events_total.copy_to(other.close_events_total);

        rx_message_bytes.copy_to(other.rx_message_bytes);

        fragments_per_message.copy_to(other.fragments_per_message);

        rx_fragments_total.copy_to(other.rx_fragments_total);
    }


    inline void debug_dump(std::ostream& os) const noexcept {

        os << "\n=== WebSocket Telemetry ===\n";
        // ---------------------------------------------------------------------
        // Traffic (cumulative)
        // ---------------------------------------------------------------------
        const uint64_t rx_msgs = messages_rx_total.load();
        os << "Traffic\n";
        os << "  RX bytes:         " << lcr::format_bytes(bytes_rx_total.load()) << '\n';
        os << "  TX bytes:         " << lcr::format_bytes(bytes_tx_total.load()) << '\n';
        os << "  RX messages:      " << lcr::format_number_exact(rx_msgs) << '\n';
        os << "  TX messages:      " << lcr::format_number_exact(messages_tx_total.load()) << '\n';

        // ---------------------------------------------------------------------
        // Errors & lifecycle
        // ---------------------------------------------------------------------
        os << "\nErrors / lifecycle\n";
        os << "  Receive errors:   " << lcr::format_number_exact(receive_errors_total.load()) << '\n';
        os << "  Close events  :   " << lcr::format_number_exact(close_events_total.load()) << '\n';

        // ---------------------------------------------------------------------
        // Message shape
        // ---------------------------------------------------------------------
        os << "\nMessage shape\n";
        os << "  RX message bytes: " << rx_message_bytes.str() << '\n';
        os << "  Fragments/msg   : " << fragments_per_message.str() << '\n';

        // ---------------------------------------------------------------------
        // Received fragments
        // ---------------------------------------------------------------------
        os << "\nFragments total\n";
        os << "  RX fragments :   " << lcr::format_number_exact(rx_fragments_total.load()) << '\n';
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

#pragma once

#include <type_traits>

#include "lcr/metrics/atomic/counter.hpp"
#include "lcr/metrics/atomic/stats/size.hpp"
#include "lcr/metrics/atomic/stats/sampler.hpp"
#include "lcr/format.hpp"

namespace wirekrak {
namespace transport {
namespace telemetry {

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
    // Advanced metrics (L3 telemetry)
    // ---------------------------------------------------------------------
    lcr::metrics::atomic::counter64 rx_assembly_time_ns;
    lcr::metrics::atomic::counter64 rx_messages_assembled_total;

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

        rx_assembly_time_ns.copy_to(other.rx_assembly_time_ns);
        rx_messages_assembled_total.copy_to(other.rx_messages_assembled_total);
    }

    // helper mutators for event signaling (called by transport implementations)
    inline void on_send(std::size_t bytes) noexcept {
        bytes_tx_total.inc(bytes);
        messages_tx_total.inc();
    }

    inline void on_receive(std::size_t bytes) noexcept {
       bytes_rx_total.inc(bytes);
    }

    inline void on_receive_failure() noexcept {
        receive_errors_total.inc();
    }

    inline void on_receive_message(std::size_t msg_size, uint32_t fragments) noexcept {
        rx_message_bytes.set(msg_size);
        messages_rx_total.inc();
        fragments_per_message.record(fragments);
    }

    inline void on_message_assembly(std::size_t msg_size) noexcept {
        rx_message_bytes.set(msg_size);
    }

    inline void on_message_assembly_copy(uint64_t duration_ns) noexcept {
        rx_assembly_time_ns.inc(duration_ns);
        rx_messages_assembled_total.inc();
    }

    inline void on_close_event() noexcept {
        close_events_total.inc();
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
        // Transport diagnostics (L3)
        // ---------------------------------------------------------------------
#ifdef WIREKRAK_ENABLE_TELEMETRY_L3
        const uint64_t assembled = rx_messages_assembled_total.load();
        const uint64_t fast_path = (rx_msgs >= assembled) ? (rx_msgs - assembled) : 0;
        const double fast_path_pct = (rx_msgs != 0) ? (100.0 * static_cast<double>(fast_path) / static_cast<double>(rx_msgs)) : 0.0;
        const uint64_t assembling_avg_cost = (assembled != 0) ? (rx_assembly_time_ns.load() / assembled) : 0;

        os << "\nTransport diagnostics (L3)\n";
        os << "  RX assembly time      :   " << lcr::format_duration(rx_assembly_time_ns.load()) << '\n';
        os << "  RX messages assembled :   " << lcr::format_number_exact(assembled) << '\n';
        os << "  Assembling avg cost   :   " << lcr::format_duration(assembling_avg_cost) << '\n';
        os << "  Fast-path messages    :   " << lcr::format_number_exact(fast_path) << " (" << std::fixed << std::setprecision(2) << fast_path_pct << "%)\n";
#endif // WIREKRAK_ENABLE_TELEMETRY_L3
    }
};

// -------------------------------------------------------------------------
// Invariants (safe, non-fragile)
// -------------------------------------------------------------------------
static_assert(std::is_standard_layout_v<WebSocket>, "telemetry::WebSocket must be standard layout");
static_assert(std::is_trivially_destructible_v<WebSocket>, "telemetry::WebSocket must be trivially destructible");
static_assert(!std::is_polymorphic_v<WebSocket>, "telemetry::WebSocket must not be polymorphic");
static_assert(alignof(WebSocket) == 64, "telemetry::WebSocket must be cache-line aligned");

} // namespace telemetry
} // namespace transport
} // namespace wirekrak

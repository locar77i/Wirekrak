#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <locale>
#include <csignal>

#include "wirekrak/core/transport/winhttp/websocket.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/subscribe.hpp"
#include "lcr/metrics/snapshot/manager.hpp"
#include "lcr/format.hpp"
#include "lcr/log/logger.hpp"

using namespace wirekrak::core;
using namespace lcr::log;


/*
===============================================================================
WinHTTP WebSocket Transport – Benchmark Commentary
===============================================================================

This benchmark evaluates the performance characteristics of the WinHTTP-based
WebSocket transport under sustained, real-world Kraken market data load.

Test conditions (representative):
  - ~4.5 million messages received
  - ~950 MB total RX traffic
  - Mixed workload: frequent small updates + rare large snapshots
  - RX buffer size: 8 KB
  - Zero-copy fast path enabled via std::string_view callbacks

-------------------------------------------------------------------------------
Key architectural observations
-------------------------------------------------------------------------------

1. Fragmentation behavior
-------------------------
WebSocket fragmentation is server-driven (RFC 6455 framing), not buffer-driven.

Observed metrics:
  - Average fragments per message ≈ 1.0007
  - Max fragments observed ≈ 11
  - Fragmented messages ≈ 0.15% of total

This confirms:
  - WinHTTP correctly preserves WebSocket framing semantics
  - Fragmentation occurs primarily for large snapshot messages
  - RX buffer size does not induce artificial fragmentation

-------------------------------------------------------------------------------
2. Zero-copy fast path effectiveness
------------------------------------
By switching the message callback signature to std::string_view, unfragmented
messages bypass all intermediate copying and allocation.

Observed metrics:
  - Fast-path messages ≈ 99.85%
  - Assembled (fragmented) messages ≈ 0.15%

This demonstrates:
  - The transport is overwhelmingly zero-copy in steady state
  - Assembly logic is only exercised when strictly required
  - Transport overhead is effectively eliminated for the common case

-------------------------------------------------------------------------------
3. Assembly cost isolation
--------------------------
Assembly cost is measured only for fragmented messages (diagnostic / L3).

Observed metrics:
  - Total RX assembly time ≈ 26 ms
  - Average assembly cost ≈ 3.7 µs per fragmented message
  - Total messages processed ≈ 4.5 million

Conclusion:
  - Assembly cost is bounded, predictable, and negligible in aggregate
  - No assembly cost is amortized across unfragmented messages
  - Transport CPU cost remains flat as message volume scales

-------------------------------------------------------------------------------
4. RX buffer sizing
-------------------
An 8 KB receive buffer provides the best balance for this workload:

  - Small enough for cache-friendly operation
  - Large enough to avoid excessive WinHTTP receive calls
  - Snapshot bursts handled correctly without pathological behavior

Increasing buffer size beyond 8–16 KB shows no measurable benefit, while
reducing cache locality.

-------------------------------------------------------------------------------
Final conclusion
-------------------------------------------------------------------------------
At scale, the WinHTTP WebSocket transport exhibits:

  - Stable throughput
  - Correct framing semantics
  - Near-total zero-copy message delivery
  - Isolated and bounded assembly overhead
  - No transport-level performance bottlenecks

Further performance work should focus on protocol parsing and downstream
application logic, not the transport layer.
===============================================================================
*/


namespace term {

inline void clear_line() {
    std::cout << "\033[2K";
}

inline void cursor_up(int n = 1) {
    std::cout << "\033[" << n << "A";
}

inline void hide_cursor() {
    std::cout << "\033[?25l";
}

inline void show_cursor() {
    std::cout << "\033[?25h";
}

inline void clear_screen() {
    std::cout << "\033[2J\033[H";
}

} // namespace term


// -----------------------------------------------------------------------------
// Ctrl+C handling
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}


int main() {
    Logger::instance().set_level(Level::Info);

    std::signal(SIGINT, on_signal);  // Handle Ctrl+C

    term::clear_screen();
    term::hide_cursor();

    // Initialize telemetry and WebSocket
    transport::telemetry::WebSocket telemetry;
    transport::winhttp::WebSocket ws(telemetry);

    // Create a telemetry manager to report the metrics
    lcr::metrics::snapshot::Manager<transport::telemetry::WebSocket> telemetry_mgr{telemetry};

/*
    ws.set_message_callback([](const std::string& msg){
        std::cout << "Received: " << msg << std::endl;
    });
*/

    std::cout << "[WS] Connecting to ws.kraken.com ..." << std::endl;
    if (ws.connect("ws.kraken.com", "443", "/v2") != transport::Error::None) {
        std::cerr << "Connect failed for 'ws.kraken.com'" << std::endl;
        return 1;
    }

    // wait a bit for messages
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // -------------------------------------------------------------------------
    // Subscribe to BOOK channel with SNAPSHOT
    // Use the intrument with more volume to see telemetry effects
    // -------------------------------------------------------------------------
    protocol::kraken::schema::book::Subscribe subscription{
        .symbols = {"BTC/USD", "BTC/EUR", "ETH/USD", "ETH/EUR", "SOL/USD", "XRP/USD", "ADA/USD", "DOGE/USD", "AVAX/USD", "LINK/USD", "DOT/USD", "MATIC/USD", "LTC/USD"},
        .depth = 1000,
        .snapshot = true
    }; 
    bool result = ws.send(subscription.to_json());
    if (!result) {
        std::cerr << "[Kraken] Subscribe failed for 'book' channel" << std::endl;
        return 2;
    }
    std::cout << "[Kraken] Subscribed to 'book' channel. Waiting for messages... (Ctrl+C to exit)" << std::endl;

    auto dump_metrics = [&](const char* tag) {
        static bool first = true;
        static uint64_t last_ts = 0;
        static uint64_t last_rx = 0;
        static uint64_t last_tx = 0;

        telemetry_mgr.take_snapshot();
        auto snapshot = telemetry_mgr.snapshot();
        const auto now = snapshot.timestamp_ns;
        const auto& m = *snapshot.data;

        if (last_ts == 0) {
            last_ts = now;
            last_rx = m.bytes_rx_total.load();
            last_tx = m.bytes_tx_total.load();
            std::cout << "[starting]\n";
            return;
        }

        const double secs = (now - last_ts) / 1e9;
        const double rx_rate = (m.bytes_rx_total.load() - last_rx) / secs;
        const double tx_rate = (m.bytes_tx_total.load() - last_tx) / secs;

        if (!first) {
            term::cursor_up(4);   // number of lines we print
        } else {
            term::cursor_up(1);
            first = false;
        }

        term::clear_line();
        std::cout << "[" << tag << "]\n";

        term::clear_line();
        std::cout << "  RX rate: " << lcr::format_throughput(rx_rate, "B/s") << '\n';

        term::clear_line();
        std::cout << "  TX rate: " << lcr::format_throughput(tx_rate, "B/s") << '\n';

        term::clear_line();
        std::cout << "  RX msgs: " << lcr::format_number_exact(m.messages_rx_total.load()) << '\n';

        std::cout << std::flush;

        last_ts = now;
        last_rx = m.bytes_rx_total.load();
        last_tx = m.bytes_tx_total.load();
    };

    // Keep running until interrupted
    // Benchmark control loop (time-driven, low overhead)
    using clock = std::chrono::steady_clock;
    constexpr auto dump_interval = std::chrono::seconds(5);
    auto next_dump = clock::now();
    while (running.load(std::memory_order_relaxed)) {
        dump_metrics("running");
        next_dump += dump_interval;
        std::this_thread::sleep_until(next_dump);
    }

    telemetry_mgr.take_snapshot();
    auto snapshot = telemetry_mgr.snapshot();
    const auto& metrics = *snapshot.data;
    metrics.debug_dump(std::cout);
    // Compute derived metrics
    const uint64_t rx_msgs = metrics.messages_rx_total.load();
    const uint64_t fragments = metrics.rx_fragments_total.load();
    const uint64_t fast_path = (rx_msgs >= fragments) ? (rx_msgs - fragments) : 0;
    const double fast_path_pct = (rx_msgs != 0) ? (100.0 * static_cast<double>(fast_path) / static_cast<double>(rx_msgs)) : 0.0;
    std::cout << "\nDerived metrics\n";
    std::cout << "  Fast-path messages  :   " << lcr::format_number_exact(fast_path) << " (" << std::fixed << std::setprecision(2) << fast_path_pct << "%)\n";

    ws.close();

    // Restore terminal state
    term::show_cursor();
    return 0;
}

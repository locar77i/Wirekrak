#pragma once

#include <cstdint>
#include <algorithm>
#include <ostream>

namespace lcr {
namespace metrics {

// ---------------------------------------------------------------------------
// burst_profiler
// ---------------------------------------------------------------------------
// - Zero allocations
// - O(1) hot path
// ---------------------------------------------------------------------------
struct alignas(64) burst_profiler {

    // -----------------------------------------------------------------------
    // Config
    // -----------------------------------------------------------------------
    static constexpr uint64_t WINDOW_NS = 50'000; // 50 µs

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    uint64_t window_start_ns{0};
    uint32_t window_count{0};

    uint32_t max_events_per_window{0};
    uint64_t total_windows{0};

    uint64_t total_events{0};

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------
    burst_profiler() noexcept = default;

    burst_profiler(const burst_profiler&) = delete;
    burst_profiler& operator=(const burst_profiler&) = delete;

    inline void copy_to(burst_profiler& dst) const noexcept {
        dst.window_start_ns       = window_start_ns;
        dst.window_count          = window_count;
        dst.max_events_per_window = max_events_per_window;
        dst.total_windows         = total_windows;
        dst.total_events          = total_events;
    }

    // -----------------------------------------------------------------------
    // Hot path (ULL-critical)
    // -----------------------------------------------------------------------
    inline void record(uint64_t now_ns) noexcept {
        total_events++;

        // First event initialization
        if (window_start_ns == 0) {
            window_start_ns = now_ns;
            window_count    = 1;
            return;
        }

        // Window rollover
        if (now_ns - window_start_ns > WINDOW_NS) {
            if (window_count > 0) {
                max_events_per_window = std::max(max_events_per_window, window_count);
                total_windows++;
            }

            window_start_ns = now_ns;
            window_count    = 1;
            return;
        }

        // Same window
        window_count++;
    }

    // -----------------------------------------------------------------------
    // Derived metrics (snapshot-safe)
    // -----------------------------------------------------------------------
    inline uint32_t max_events_observed() const noexcept {
        return std::max(max_events_per_window, window_count);
    }

    inline uint64_t total_windows_observed() const noexcept {
        return total_windows + (window_count > 0 ? 1 : 0);
    }

    inline double avg_events_per_window() const noexcept {
        const uint64_t windows = total_windows_observed();
        return windows > 0
            ? static_cast<double>(total_events) / static_cast<double>(windows)
            : 0.0;
    }

    inline double peak_rate_per_sec() const noexcept {
        const double window_sec = WINDOW_NS / 1e9;
        return window_sec > 0
            ? static_cast<double>(max_events_observed()) / window_sec
            : 0.0;
    }

    // -----------------------------------------------------------------------
    // Dump (presentation layer → still domain-friendly)
    // -----------------------------------------------------------------------
    inline void dump(std::ostream& os) const noexcept {
        const uint32_t max_eff = max_events_observed();

        os << "Microbursts (" << WINDOW_NS / 1000 << " us window)\n";
        os << "  Max burst size       : " << max_eff << " events\n";
        os << "  Total bursts         : " << total_windows_observed() << '\n';
        os << "  Avg burst size       : " << avg_events_per_window() << " events\n";

        os << "\nBurst intensity\n";
        os << "  Peak rate (window)   : "
           << static_cast<uint64_t>(peak_rate_per_sec()) << " events/s\n";
    }

    // -----------------------------------------------------------------------
    // Reset
    // -----------------------------------------------------------------------
    inline void reset() noexcept {
        window_start_ns       = 0;
        window_count          = 0;
        max_events_per_window = 0;
        total_windows         = 0;
        total_events          = 0;
    }

    // -----------------------------------------------------------------------
    // Collector
    // -----------------------------------------------------------------------
    template <typename Collector>
    void collect(const std::string& name, Collector& collector) const noexcept {
        collector.add_gauge((double)max_events_observed(),
            name + "_max_events_per_window", "Max events in a microburst window");

        collector.add_gauge((double)total_windows_observed(),
            name + "_total_windows", "Total burst windows observed");

        collector.add_gauge(avg_events_per_window(),
            name + "_avg_events_per_window", "Average events per window");

        collector.add_gauge(peak_rate_per_sec(),
            name + "_peak_rate_per_sec", "Peak event rate extrapolated to events/s");
    }
};

} // namespace metrics
} // namespace lcr

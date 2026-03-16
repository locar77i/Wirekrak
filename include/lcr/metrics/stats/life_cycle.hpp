#pragma once

#include <string>
#include <sstream>
#include <atomic>
#include <type_traits>
#include <cstdint>

#include "lcr/metrics/counter.hpp"
#include "lcr/metrics/gauge.hpp"
#include "lcr/time_unit.hpp"

namespace lcr {
namespace metrics {
namespace stats {

// ---------------------------------------------------------------------------
// life_cycle
// ---------------------------------------------------------------------------
// Tracks the lifecycle of long-running maintenance-style threads.
// Suitable for retention, compaction, flush daemons, etc.
//
// No multithreading guarantees — use only from single thread or via snapshot copies.
// ---------------------------------------------------------------------------
struct alignas(64) life_cycle {
    // Constructor
    life_cycle() = default;
    // Disable copy/move semantics
    life_cycle(const life_cycle&) = delete;
    life_cycle& operator=(const life_cycle&) = delete;
    life_cycle(life_cycle&&) noexcept = delete;
    life_cycle& operator=(life_cycle&&) noexcept = delete;

    // Specialized copy method
    void copy_to(life_cycle& other) const noexcept {
        other.cycle_count_.store(cycle_count_.load());
        other.did_work_total_.store(did_work_total_.load());
        other.total_cycle_time_ns_.store(total_cycle_time_ns_.load());
        other.total_active_time_ns_.store(total_active_time_ns_.load());
        other.last_sleep_ms_.store(last_sleep_ms_.load());
    }

    // Record a full maintenance loop
    inline void record(uint64_t cycle_ns, uint64_t sleep_ns, bool did_work) noexcept {
        cycle_count_.inc();
        total_cycle_time_ns_.add(cycle_ns);
        if (did_work) {
            uint64_t active_ns = cycle_ns - sleep_ns;
            total_active_time_ns_.add(active_ns);
            did_work_total_.inc();
        }
        last_sleep_ms_.store(static_cast<uint32_t>(sleep_ns / 1'000'000ULL));
    }

    inline void record(uint64_t start_ns, uint64_t end_ns, uint64_t sleep_ns, bool did_work) noexcept {
        record(end_ns - start_ns, sleep_ns, did_work);
    }

    // Derived metrics
    inline double active_ratio() const noexcept {
        const uint64_t total = total_cycle_time_ns_.load();
        if (total == 0) return 0.0;
        return 100.0 * static_cast<double>(total_active_time_ns_.load()) / static_cast<double>(total);
    }

    inline uint64_t avg_cycle_ns() const noexcept {
        const uint64_t n = cycle_count_.load();
        if (n == 0) return 0;
        return total_cycle_time_ns_.load() / n;
    }

    inline uint64_t avg_active_ns() const noexcept {
        const uint64_t n = cycle_count_.load();
        if (n == 0) return 0;
        return total_active_time_ns_.load() / n;
    }

    // Reset (single-threaded only)
    inline void reset() noexcept {
        cycle_count_.reset();
        did_work_total_.reset();
        total_cycle_time_ns_.reset();
        total_active_time_ns_.reset();
        last_sleep_ms_.reset();
    }

    inline void dump(std::ostream& os) const noexcept {
        os << "cycles=" << cycle_count_.load()
            << " did_work=" << did_work_total_.load()
            << " total=" << lcr::format_duration(total_cycle_time_ns_.load())
            << " active=" << lcr::format_duration(total_active_time_ns_.load())
            << " ratio=" << active_ratio() << "%"
            << " avg_cycle=" << lcr::format_duration(avg_cycle_ns())
            << " avg_active=" << lcr::format_duration(avg_active_ns())
            << " last_sleep_ms=" << last_sleep_ms_.load() << "ms";
    }

    // String formatter (for debug/logs)
    inline std::string str() const noexcept {
        std::ostringstream os;
        dump(os);
        return os.str();
    }

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& prefix, Collector& collector) const noexcept {
        cycle_count_.collect(prefix + "_cycles", "Total number of maintenance cycles", collector);
        did_work_total_.collect(prefix + "_did_work_total", "Total number of cycles that did work", collector);
        total_cycle_time_ns_.collect(prefix + "_total_cycle_time_ns", "Cumulative cycle time in nanoseconds", collector);
        total_active_time_ns_.collect(prefix + "_total_active_time_ns", "Cumulative active time in nanoseconds", collector);
        last_sleep_ms_.collect(prefix + "_last_sleep_ms", "Duration of last sleep in milliseconds", collector);
        // Derived ratios/averages
        collector.add_gauge(active_ratio(), prefix + "_active_ratio_percent", "Percentage of time active during cycles");
        collector.add_gauge(convert_ns(avg_cycle_ns(), time_unit::milliseconds), prefix + "_avg_cycle_time_ms", "Average cycle time in milliseconds");
        collector.add_gauge(convert_ns(avg_active_ns(), time_unit::milliseconds), prefix + "_avg_active_time_ms", "Average active time in milliseconds");
    }

private:
    counter64 cycle_count_{};
    counter64 did_work_total_{};
    counter64 total_cycle_time_ns_{};
    counter64 total_active_time_ns_{};
    gauge32   last_sleep_ms_{};
};

} // namespace stats
} // namespace metrics
} // namespace lcr

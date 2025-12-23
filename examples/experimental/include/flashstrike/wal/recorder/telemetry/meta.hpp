#pragma once

#include <string>
#include <ostream>

#include "flashstrike/wal/types.hpp"
#include "lcr/metrics.hpp"
#include "lcr/system/monotonic_clock.hpp"

using namespace lcr::metrics;


namespace flashstrike {
namespace wal {
namespace recorder {
namespace telemetry {

struct alignas(64) MetaStore {
    alignas(64) stats::duration64 maintenance_meta_flush{};

    // Constructor
    MetaStore() = default;
    // Disable copy/move semantics
    MetaStore(const MetaStore&) = delete;
    MetaStore& operator=(const MetaStore&) = delete;
    MetaStore(MetaStore&&) noexcept = delete;
    MetaStore& operator=(MetaStore&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(MetaStore& other) const noexcept {
        maintenance_meta_flush.copy_to(other.maintenance_meta_flush);
    }

    // Dump metrics to ostream (human-readable)
    inline void dump(const std::string& label, std::ostream& os) const noexcept {
        os  << "[" << label << " Metrics] Snapshot:\n";
        os << "-----------------------------------------------------------------\n";
        os << " Meta flush: " << maintenance_meta_flush.str() << "\n";
        os << "-----------------------------------------------------------------\n";
    }

    // Metrics collector
    template <typename Collector>
    inline void collect(const std::string& prefix, Collector& collector) const noexcept {
        // Push the current label before serializing
        collector.push_label("subsystem", "wal_meta");
        // Serialize meta metrics
        maintenance_meta_flush.collect(prefix + "_maintenance_meta_flush", collector);
        // Pop the label after serialization
        collector.pop_label();
    }
};
// Compile-time verification
static_assert(sizeof(MetaStore) % 64 == 0, "MetaStore size must be multiple of 64 bytes");
static_assert(alignof(MetaStore) == 64, "MetaStore must be aligned to 64 bytes");
static_assert(offsetof(MetaStore, maintenance_meta_flush) % 64 == 0, "maintenance_meta_flush must start at a cache-line boundary");

class MetaUpdater {
public:
    explicit MetaUpdater(MetaStore& metrics) : metrics_(metrics) {}

    inline void on_async_meta_flush_completed(uint64_t start_ns) const noexcept {
        metrics_.maintenance_meta_flush.record(start_ns, monotonic_clock::instance().now_ns());
    }

private:
    MetaStore& metrics_;
};


} // namespace telemetry
} // namespace recorder
} // namespace wal
} // namespace flashstrike

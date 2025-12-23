#pragma once

#include <vector>
#include <cstdint>
#include <limits>

#include "flashstrike/constants.hpp"
#include "flashstrike/types.hpp"
#include "flashstrike/matching_engine/telemetry/init.hpp"
#include "flashstrike/matching_engine/telemetry/low_level.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/memory/footprint.hpp"

using lcr::system::monotonic_clock;


namespace flashstrike {
namespace matching_engine {

using ScrambledId = std::uint32_t; // Scrambled order id for hashing
using HashMapIdx = std::int32_t;  // Index in the hash map table

// Load factor multiplier to size the hash map
static constexpr float LOAD_FACTOR_MULTIPLIER = 2.0f; // 0.5 load factor

// Simple open-addressing hash map mapping orderId -> pool index, neccessary because orders are externally addressable by id.
// Provides constant-time access to orders by their unique ID, making cancels, modifies, and executions efficient enough for production-scale trading systems.
// Uses open addressing with linear probing.
// Enables O(1) lookup for cancel/modify.
class OrderIdMap {
public:
    // Constructor initializes the hash table with a given capacity.
    // Marks all entries as empty (key=0) and sets the size.
    // With load factor 0.25 we can achieve low probe chains under cancellations/reinserts.
    // No resizing. Fixed size is fine if we don't mis-estimate capacity: the map would simply fail.
    explicit OrderIdMap(std::uint64_t capacity, telemetry::Init& init_metrics, telemetry::LowLevel& low_level_metrics)
        : init_metrics_updater_(init_metrics)
        , low_level_metrics_updater_(low_level_metrics)
    {
        auto start_ns = monotonic_clock::instance().now_ns();
        capacity *= LOAD_FACTOR_MULTIPLIER;
        if (capacity < 16) capacity = 16; // minimum size
        // round up to next power of two
        capacity_ = 1;
        while (capacity_ < capacity) capacity_ <<= 1;  // round up to power of two
        mask_ = capacity_ - 1;
        table_.resize(capacity_);
        for (auto &e : table_) {
            e.key = EMPTY_KEY;
            e.val = INVALID_INDEX;
        }
        init_metrics_updater_.on_create_order_id_map(start_ns, capacity_, memory_usage().total_bytes());
    }
    // Non-copyable
    OrderIdMap(const OrderIdMap&) = delete;
    OrderIdMap& operator=(const OrderIdMap&) = delete;
    // Non-moveable
    OrderIdMap(OrderIdMap&&) noexcept = delete;
    OrderIdMap& operator=(OrderIdMap&&) noexcept = delete;

    [[nodiscard]] inline std::uint64_t capacity() const noexcept {
        return capacity_;
    }

    [[nodiscard]] inline std::uint64_t used() const noexcept {
        return size_;
    }

    [[nodiscard]] inline std::uint64_t free_slots() const noexcept {
        return capacity_ - size_;
    }

    // Finds the pool index for a given orderId.
    // Returns the pool index or INVALID_INDEX if not found.
    [[nodiscard]] inline OrderIdx find(OrderId ordid) const noexcept {
        ScrambledId h = hash_orderId_(ordid) & mask_;
        for (std::uint64_t i = 0; i < capacity_; ++i) { // linear probing
            HashMapIdx idx = (h + i) & mask_;
            if (table_[idx].key == EMPTY_KEY) return INVALID_INDEX; // empty → not found
            if (table_[idx].key == ordid) return table_[idx].val; // found
        }
        return INVALID_INDEX;
    }

    [[nodiscard]] inline bool contains(OrderId id) const noexcept {
        return find(id) != INVALID_INDEX;
    }

    // Inserts a mapping from orderId to pool index and returns true if successful, false if the table is full.
    // Warning! We uses linear probing for collision resolution.
    // Even with low load factor, if IDs are sequential and hash poorly, probe chains may get long (Linear probing clustering).
    // Usually it's not an issue with a 25% load factor, but worth watching.
    [[nodiscard]] inline bool insert(OrderId ordid, OrderIdx order_idx) noexcept {
#ifdef ENABLE_FS3_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        //if (size_ * LOAD_FACTOR_MULTIPLIER >= capacity_) return false;
        ScrambledId h = hash_orderId_(ordid) & mask_;
        std::uint64_t i = 0;
        for (; i < capacity_; ++i) { // linear probing
            HashMapIdx idx = (h + i) & mask_;
            if (table_[idx].key == EMPTY_KEY || table_[idx].key == TOMBSTONE_KEY) {
                table_[idx].key = ordid;
                table_[idx].val = order_idx;
                ++size_;
#ifdef ENABLE_FS3_METRICS
                low_level_metrics_updater_.on_insert_ordid(start_ns, true, i);
#endif
                return true;
            }
        }
#ifdef ENABLE_FS3_METRICS
        low_level_metrics_updater_.on_insert_ordid(start_ns, false, i);
#endif
        return false; // table full
    }

    // Removes a mapping for a given orderId.
    // Returns true if successful, false if not found.
    inline bool remove(OrderId ordid) noexcept {
#ifdef ENABLE_FS3_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        ScrambledId h = hash_orderId_(ordid) & mask_;
        std::uint64_t i = 0;
        for (; i < capacity_; ++i) { // linear probing
            HashMapIdx idx = (h + i) & mask_;
            if (table_[idx].key == EMPTY_KEY) { // not found
#ifdef ENABLE_FS3_METRICS
                low_level_metrics_updater_.on_remove_ordid(start_ns, false, i);
#endif
                return false;
            }
            if (table_[idx].key == ordid) {
                table_[idx].key = TOMBSTONE_KEY;
                table_[idx].val = INVALID_INDEX;
                --size_;
#ifdef ENABLE_FS3_METRICS
                low_level_metrics_updater_.on_remove_ordid(start_ns, true, i);
#endif
                return true;
            }
        }
#ifdef ENABLE_FS3_METRICS
        low_level_metrics_updater_.on_remove_ordid(start_ns, false, i);
#endif
        return false; // not found
    }

    inline void clear() noexcept {
        for (auto &e : table_) {
            e.key = EMPTY_KEY;
            e.val = INVALID_INDEX;
        }
        size_ = 0;
    }

    inline lcr::memory::footprint memory_usage() const noexcept {
        lcr::memory::footprint mf{
            .static_bytes = sizeof(OrderIdMap),
            .dynamic_bytes = table_.capacity() * sizeof(Entry)
        };
        return mf;
    }

private:
    struct Entry {
        OrderId key;   // order id
        OrderIdx val;  // order pool index
    };

    static constexpr OrderId EMPTY_KEY = 0;
    static constexpr OrderId TOMBSTONE_KEY = std::numeric_limits<OrderId>::max();

    std::vector<Entry> table_;
    std::uint64_t capacity_;     // must be power of two
    std::int32_t mask_;
    std::uint64_t size_{0};      // for load factor control

    // METRICS --------------------------------------------------------
    telemetry::InitUpdater init_metrics_updater_;
    telemetry::LowLevelUpdater low_level_metrics_updater_;

    // Internal helpers -------------------------------------------------------------------------

    // Simple fast hash to scramble sequential IDs (Knuth multiplicative hash)
    inline ScrambledId hash_orderId_(OrderId ordid) const noexcept {
        return (ScrambledId)ordid * 2654435761u;
    }
};

} // namespace matching_engine
} // namespace flashstrike

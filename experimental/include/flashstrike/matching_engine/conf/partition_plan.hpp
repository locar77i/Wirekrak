#pragma once

#include <cstdint>
#include <string>
#include <ostream>
#include <sstream>
#include <type_traits>

#include "flashstrike/matching_engine/conf/instrument.hpp"
#include "flashstrike/matching_engine/conf/normalized_instrument.hpp"
#include "lcr/normalization.hpp"
#include "lcr/numbers.hpp"


namespace flashstrike {
namespace matching_engine {
namespace conf {

    // ============================================================================
    //  PartitionPlan
    //  ----------------------------------------------------------------------------
    //  Pure internal representation of how prices are discretized and organized
    //  in memory.  It defines *only* the derived, integer-based layout used by
    //  the MatchingEngine and PriceLevelStores.
    //
    //  - It does not know about units (USD, BTC) or decimals.
    //  - It is always generated from a Instrument.
    //  - It describes how the price continuum is split into contiguous partitions
    //    and how many discrete ticks exist in total.
    //
    //  The MatchingEngine uses it for:
    //    * Price → Partition index mapping
    //    * Tick-level computations
    //    * Preallocation and sizing of data structures
    // ============================================================================
    struct PartitionPlan {
        // Accessors
        inline constexpr std::uint32_t partition_bits() const noexcept { return partition_bits_; }
        inline constexpr std::uint32_t num_partitions() const noexcept { return num_partitions_; }
        inline constexpr std::uint64_t partition_size() const noexcept { return partition_size_; }
        inline constexpr std::uint64_t num_ticks() const noexcept { return num_ticks_; }

        // User-facing partition plan computation (safe, from Instrument)
        inline NormalizedInstrument compute(const Instrument& instrument, std::uint32_t target_num_partitions) noexcept {
            assert(instrument.price_tick_units > 0.0 && "instrument.price_tick_units must be > 0");
            assert(instrument.price_max_units > instrument.price_tick_units && "price_max_units must be > price_tick_units");
            assert(target_num_partitions > 0 && "target_num_partitions must be > 0");
            // Normalize tick size into integer units
            Price price_tick_size;
            std::uint64_t scale = lcr::normalize_tick_size(instrument.price_tick_units, price_tick_size);
            assert(price_tick_size > 0 && "price_tick_size must be > 0");
            // Scale max price by the same multiplier
            Price price_max_scaled = static_cast<Price>(instrument.price_max_units * scale);
            assert(price_max_scaled > price_tick_size && "price_max_scaled must be > price_tick_size");
            assert((price_max_scaled / price_tick_size) <= std::numeric_limits<std::int64_t>::max() && "num_ticks_ would overflow 64-bit integer range");
            // Compute total number of discrete ticks (price levels)
            num_ticks_ = static_cast<std::uint64_t>(price_max_scaled / price_tick_size);
            assert(num_ticks_ > 0 && "num_ticks_ must be positive");
            // Round both dimensions to power-of-two values for partitioning efficiency
            num_ticks_ = lcr::round_up_to_power_of_two_64(num_ticks_);
            target_num_partitions = lcr::round_up_to_power_of_two_32(target_num_partitions);
            // Compute partition size (#ticks per partition) and its log2 representation
            assert(target_num_partitions <= num_ticks_ && "target_num_partitions must be <= num_ticks_");
            num_partitions_ = target_num_partitions;
            partition_size_ = num_ticks_ / num_partitions_;
            assert(partition_size_ > 0 && "partition_size_ must be > 0");
            partition_bits_ = __builtin_ctzll(partition_size_); // log2(partition_size_)
            assert(partition_bits_ < (sizeof(Price) * 8 - 1) && "partition_bits_ too large for Price type");
            return instrument.normalize(num_ticks_);
        }

        inline void debug_dump(std::ostream& os) const {
            os << "[PartitionPlan]:\n"
            << "  Partition Bits      : " << partition_bits_ << "\n"
            << "  Number of Partitions: " << num_partitions_ << "\n"
            << "  Partition Size      : " << partition_size_ << "\n"
            << "  Number of Ticks     : " << num_ticks_ << "\n"
            ;
        }
        inline std::string to_string() const {
            std::ostringstream oss;
            debug_dump(oss);
            return oss.str();
        }

    private:
        std::uint32_t partition_bits_{};   // log2(partition_size)
        std::uint32_t num_partitions_{};   // number of partitions in total
        std::uint64_t partition_size_{};   // number of ticks per partition (power of two)
        std::uint64_t num_ticks_{};        // total discrete ticks across price range
    };

    static_assert(std::is_trivially_copyable_v<PartitionPlan>);
    static_assert(std::is_standard_layout_v<PartitionPlan>);


} // namespace conf
} // namespace matching_engine
} // namespace flashstrike

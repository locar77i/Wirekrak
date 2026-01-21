#pragma once


#include <cstdint>
#include <cstring>
#include <cstddef>
#include <type_traits>

#include <xxhash.h>

#include "flashstrike/wal/types.hpp"
#include "flashstrike/wal/constants.hpp"
#include "lcr/endian.hpp"
#include "lcr/log/logger.hpp"


namespace flashstrike {
namespace wal {
namespace segment {

// ---------------------------------------------------------------------------
// Header for a single WAL block (aligned to 64 B)
// ---------------------------------------------------------------------------
struct alignas(64) BlockHeader {
    uint64_t first_event_id_le;      // ID of the first event in this block
    uint64_t last_event_id_le;       // ID of the last event in this block
    uint64_t block_checksum_le;      // XXH64 of events[] only
    uint64_t chained_checksum_le;    // XXH64 of events[] + prev_chained
    uint64_t checksum_le;            // XXH64 of this header (excluding this field)
    uint32_t block_index_le;         // Sequential index within segment (0,1,…)
    uint16_t event_count_le;         // Number of valid events in this block
    uint8_t  pad_[18];            // Padding → total 64 B


    // -------------------------------
    // Accessors (auto endian convert)
    // -------------------------------
    [[nodiscard]] inline uint64_t first_event_id() const noexcept { return lcr::from_le64(first_event_id_le); }
    inline void set_first_event_id(uint64_t v) noexcept { first_event_id_le = lcr::to_le64(v); }

    [[nodiscard]] inline uint64_t last_event_id() const noexcept  { return lcr::from_le64(last_event_id_le); }
    inline void set_last_event_id(uint64_t v) noexcept  { last_event_id_le  = lcr::to_le64(v); }

    [[nodiscard]] inline uint64_t block_checksum() const noexcept { return lcr::from_le64(block_checksum_le); }
    inline void set_block_checksum(uint64_t v) noexcept { block_checksum_le = lcr::to_le64(v); }

    [[nodiscard]] inline uint64_t chained_checksum() const noexcept { return lcr::from_le64(chained_checksum_le); }
    inline void set_chained_checksum(uint64_t v) noexcept { chained_checksum_le = lcr::to_le64(v); }

    [[nodiscard]] inline uint64_t checksum() const noexcept { return lcr::from_le64(checksum_le); }
    inline void set_checksum(uint64_t v) noexcept       { checksum_le = lcr::to_le64(v); }

    [[nodiscard]] inline uint32_t block_index() const noexcept { return lcr::from_le32(block_index_le); }
    inline void set_block_index(uint32_t v) noexcept    { block_index_le = lcr::to_le32(v); }

    [[nodiscard]] inline uint16_t event_count() const noexcept { return lcr::from_le16(event_count_le); }
    inline void set_event_count(uint16_t v) noexcept    { event_count_le = lcr::to_le16(v); }

    // ---------------------------------------------------------------------------
    inline void reset() noexcept {
        std::memset(this, 0, sizeof(BlockHeader));
    }
    inline void reset_pad() noexcept {
        std::memset(pad_, 0, sizeof(pad_));
    }

    // Compute checksum of the header itself (excluding checksum field)
    [[nodiscard]] static inline uint64_t compute_checksum(const BlockHeader& header) noexcept {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&header);
        constexpr size_t checksum_off = offsetof(BlockHeader, checksum_le);
        constexpr size_t checksum_size = sizeof(uint64_t);
        uint64_t h1 = XXH64(bytes, checksum_off, 0);
        return XXH64(bytes + checksum_off + checksum_size, sizeof(BlockHeader) - checksum_off - checksum_size, h1);
    }

    inline void finalize(uint32_t block_index, uint64_t block_checksum, uint64_t chained_checksum) noexcept {
        set_block_index(block_index);
        set_block_checksum(block_checksum);
        set_chained_checksum(chained_checksum);
        // Compute and store the header checksum in little-endian form
        set_checksum(compute_checksum(*this));
    }

    // Lightweight structural validation (no checksum) for WAL block header
    [[nodiscard]] inline bool validate_data() const noexcept {
        // 1. Event count must be within legal bounds
        if (event_count() == 0 || event_count() > WAL_BLOCK_EVENTS) {
            WK_TRACE("[!!] Invalid event_count in WAL block header: " << event_count());
            return false;
        }
        // 2. Event ID range must be consistent
        if (first_event_id() == 0 || last_event_id() == 0) {
            WK_TRACE("[!!] Invalid event ID range in WAL block header: " << first_event_id() << ", " << last_event_id());
            return false;
        }
        if (first_event_id() > last_event_id()) {
            WK_TRACE("[!!] Inconsistent event ID range in WAL block header: " << first_event_id() << " > " << last_event_id());
            return false;
        }
        // 3. Block index must be reasonable
        if (block_index() > MAX_BLOCKS) {
            WK_TRACE("[!!] Invalid block_index in WAL block header: " << block_index());
            return false;
        }
        return true; // All checks passed
    }

    [[nodiscard]] inline bool validate_checksum() const noexcept {
        return checksum() == compute_checksum(*this);
    }

};
// ======================================================
// Layout validation (prevent ABI drift)
// ======================================================
static_assert(alignof(BlockHeader) == 64 && "BlockHeader must be cacheline-aligned");
static_assert(sizeof(BlockHeader) == 64, "BlockHeader must be 64 bytes");
// Sanity checks for field offsets
static_assert(offsetof(BlockHeader, first_event_id_le) == 0, "first_event_id_le offset mismatch");
static_assert(offsetof(BlockHeader, last_event_id_le) == 8, "last_event_id_le offset mismatch");
static_assert(offsetof(BlockHeader, block_checksum_le) == 16, "block_checksum_le offset mismatch");
static_assert(offsetof(BlockHeader, chained_checksum_le) == 24, "chained_checksum_le offset mismatch");
static_assert(offsetof(BlockHeader, checksum_le) == 32, "checksum_le offset mismatch");
static_assert(offsetof(BlockHeader, block_index_le) == 40, "block_index_le offset mismatch");
static_assert(offsetof(BlockHeader, event_count_le) == 44, "event_count_le offset mismatch");
// Ensure POD semantics
static_assert(std::is_standard_layout<BlockHeader>::value, "BlockHeader must have standard layout");
static_assert(std::is_trivial<BlockHeader>::value, "BlockHeader must be trivially copyable");


} // namespace segment
} // namespace wal
} // namespace flashstrike

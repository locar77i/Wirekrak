#pragma once


#include <cstdint>
#include <cstring>
#include <cstddef>
#include <type_traits>

#include <xxhash.h>

#include "flashstrike/wal/types.hpp"
#include "flashstrike/wal/constants.hpp"
#include "flashstrike/wal/segment/block_header.hpp"
#include "flashstrike/events.hpp"
#include "lcr/log/logger.hpp"


namespace flashstrike {
namespace wal {
namespace segment {

// ---------------------------------------------------------------------------
// Full block (header + events)
// ---------------------------------------------------------------------------
struct alignas(64) Block {
    BlockHeader header;
    RequestEvent events[WAL_BLOCK_EVENTS];

    // ---------------------------------------------------------------------------
    inline void reset() noexcept {
        std::memset(this, 0, sizeof(Block));
    }
    inline void reset_pad() noexcept {
        header.reset_pad();
        for (size_t i = 0; i < WAL_BLOCK_EVENTS; ++i) {
            events[i].reset_pad();
        }
    }

    // ---- Compute local checksum (block integrity only) ----
    [[nodiscard]] static inline uint64_t compute_block_checksum(const RequestEvent* ev, size_t count) noexcept {
        return XXH64(ev, count * sizeof(RequestEvent), 0);
    }

    // ---- Compute chained checksum (depends on previous block) ----
    [[nodiscard]] static inline uint64_t compute_chained_checksum(const RequestEvent* ev, size_t count, uint64_t prev_chain) noexcept {
        return XXH64(ev, count * sizeof(RequestEvent), prev_chain);
    }

    // ---- Finalize this block before writing to disk ----
    inline void finalize(uint32_t block_index, uint64_t prev_chained) noexcept {
        header.finalize(block_index,
                        compute_block_checksum(events, header.event_count()),
                        compute_chained_checksum(events, header.event_count(), prev_chained)
        );
    }

    // Structural validation (no checksum) for WAL block
    [[nodiscard]] inline bool validate_data() const noexcept {
        if (!header.validate_data()) return false;
        // Event ID sequence consistency
        if (header.first_event_id() > header.last_event_id()) {
            WK_TRACE("[!!] Inconsistent event ID range in WAL block: " << header.first_event_id() << " > " << header.last_event_id());
            return false;
        }
        // Event IDs inside block must be monotonic
        for (size_t i = 1; i < header.event_count(); ++i) {
            if (events[i].event_id <= events[i - 1].event_id) {
                WK_TRACE("[!!] Non-monotonic event IDs in WAL block at index " << header.block_index() << ": event[" << i-1 << "]=" << events[i-1].event_id << ", event[" << i << "]=" << events[i].event_id);
                return false;
            }
        }
        return true;
    }

    // Validate both checksums for this block (no structural checks)
    [[nodiscard]] inline Status validate_checksums(uint64_t prev_chained) const noexcept {
        // 1. Compute event array checksum (XXH64 or CRC64)
        uint64_t local = compute_block_checksum(events, header.event_count());
        if (local != header.block_checksum()) {
            WK_TRACE("[!!] Block checksum mismatch: expected " << header.block_checksum() << ", computed " << local);
            return Status::BLOCK_CHECKSUM_MISMATCH;
        }
        // 2. Compute chained checksum: includes previous chained value
        uint64_t chained = compute_chained_checksum(events, header.event_count(), prev_chained);
        if (chained != header.chained_checksum()) {
            WK_TRACE("[!!] Chained checksum mismatch: expected " << header.chained_checksum() << ", computed " << chained);
            return Status::CHAINED_CHECKSUM_MISMATCH;
        }
        return Status::OK;
    }

    // Full block validation (structural + checksums)
    [[nodiscard]] inline Status verify(uint64_t prev_chained = 0) const noexcept {
        // Step 1: structural validation
        if (!validate_data()) return Status::SEGMENT_POSSIBLY_CORRUPTED;
        // Step 2: Block checksums (events array only)
        return validate_checksums(prev_chained);
    }

    inline void serialize(void* dest) const noexcept {
        std::memcpy(static_cast<uint8_t*>(dest), this, sizeof(Block));
    }

    inline void deserialize(const void* src) noexcept {
        std::memcpy(this, static_cast<const uint8_t*>(src), sizeof(Block));
    }

    // ---- Size helpers ----
    static constexpr size_t byte_size() noexcept {
        return sizeof(BlockHeader) + WAL_BLOCK_EVENTS * sizeof(RequestEvent);
    }
};
// ======================================================
// Layout validation (prevent ABI drift)
// ======================================================
static_assert(alignof(Block) == 64 && "Block must be cacheline-aligned");
static_assert(sizeof(Block) == sizeof(BlockHeader) + WAL_BLOCK_EVENTS * sizeof(RequestEvent), "Block size mismatch");
// sanity checks for offsets (compile-time guarantees)
static_assert(offsetof(Block, header) == 0, "Block header offset mismatch");
static_assert(offsetof(Block, events) == sizeof(BlockHeader), "Block events offset mismatch");
// Ensure POD semantics
static_assert(std::is_standard_layout<Block>::value, "Block must have standard layout");
static_assert(std::is_trivial<Block>::value, "Block must be trivially copyable");


} // namespace segment
} // namespace wal
} // namespace flashstrike
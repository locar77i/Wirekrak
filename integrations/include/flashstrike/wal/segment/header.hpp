#pragma once


#include <cstdint>
#include <cstring>
#include <cstddef>
#include <type_traits>

#include <xxhash.h>

#include "flashstrike/wal/types.hpp"
#include "flashstrike/wal/constants.hpp"
#include "flashstrike/wal/segment/block.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/endian.hpp"
#include "lcr/log/logger.hpp"

using lcr::system::monotonic_clock;


namespace flashstrike {
namespace wal {
namespace segment {

// WAL segment header (fixed size: 64 bytes, aligned to cache line).
// Stored at the beginning of every WAL file. Provides metadata for replay, corruption detection, and fast seeking.
// Purpose:
// - Identify the WAL version and encoding.
// - Anchor replay by first_event_id and last_event_id.
// - Record how many events are inside.
// - Provide quick integrity check.
struct alignas(64) Header {
    uint16_t magic_le;                //  0: 2B  - 'FS' magic or similar
    uint8_t  version_le;              //  2: 1B
    uint8_t  header_size_le;          //  3: 1B  - sizeof(Header)
    uint32_t segment_index_le;        //  4: 4B
    uint32_t block_count_le;          //  8: 4B  - number of blocks
    uint32_t event_count_le;          // 12: 4B
    uint64_t first_event_id_le;       // 16: 8B
    uint64_t last_event_id_le;        // 24: 8B
    uint64_t created_ts_ns_le;        // 32: 8B
    uint64_t closed_ts_ns_le;         // 40: 8B
    uint64_t checksum_le;             // 48: 8B
    uint64_t last_chained_checksum_le;// 56: 8B

    // -------------------------------
    // Accessors (auto endian convert)
    // -------------------------------
    [[nodiscard]] inline uint16_t magic() const noexcept { return lcr::from_le16(magic_le); }
    inline void set_magic(uint16_t v) noexcept { magic_le = lcr::to_le16(v); }

    [[nodiscard]] inline uint8_t version() const noexcept { return version_le; }
    inline void set_version(uint8_t v) noexcept { version_le = v; }

    [[nodiscard]] inline uint8_t header_size() const noexcept { return header_size_le; }
    inline void set_header_size(uint8_t v) noexcept { header_size_le = v; }

    [[nodiscard]] inline uint32_t segment_index() const noexcept { return lcr::from_le32(segment_index_le); }
    inline void set_segment_index(uint32_t v) noexcept { segment_index_le = lcr::to_le32(v); }

    [[nodiscard]] inline uint32_t block_count() const noexcept { return lcr::from_le32(block_count_le); }
    inline void set_block_count(uint32_t v) noexcept { block_count_le = lcr::to_le32(v); }

    [[nodiscard]] inline uint32_t event_count() const noexcept { return lcr::from_le32(event_count_le); }
    inline void set_event_count(uint32_t v) noexcept { event_count_le = lcr::to_le32(v); }

    [[nodiscard]] inline uint64_t first_event_id() const noexcept { return lcr::from_le64(first_event_id_le); }
    inline void set_first_event_id(uint64_t v) noexcept { first_event_id_le = lcr::to_le64(v); }

    [[nodiscard]] inline uint64_t last_event_id() const noexcept { return lcr::from_le64(last_event_id_le); }
    inline void set_last_event_id(uint64_t v) noexcept { last_event_id_le = lcr::to_le64(v); }

    [[nodiscard]] inline uint64_t created_ts_ns() const noexcept { return lcr::from_le64(created_ts_ns_le); }
    inline void set_created_ts_ns(uint64_t v) noexcept { created_ts_ns_le = lcr::to_le64(v); }

    [[nodiscard]] inline uint64_t closed_ts_ns() const noexcept { return lcr::from_le64(closed_ts_ns_le); }
    inline void set_closed_ts_ns(uint64_t v) noexcept { closed_ts_ns_le = lcr::to_le64(v); }

    [[nodiscard]] inline uint64_t checksum() const noexcept { return lcr::from_le64(checksum_le); }
    inline void set_checksum(uint64_t v) noexcept { checksum_le = lcr::to_le64(v); }

    [[nodiscard]] inline uint64_t last_chained_checksum() const noexcept { return lcr::from_le64(last_chained_checksum_le); }
    inline void set_last_chained_checksum(uint64_t v) noexcept { last_chained_checksum_le = lcr::to_le64(v); }

    // ---------------------------------------------------------------------------
    inline void reset() noexcept {
        std::memset(this, 0, sizeof(Header));
    }
    inline void reset_pad() noexcept {
        // No padding fields to reset
    }

    // Compute fast 64-bit checksum over the WAL header only.
    // For WAL headers (64B), we want super low latency, so we use stack-only XXH64:
    // - No heap allocation (XXH64_state_t not needed).
    // - Only two stack-based XXH64 calls.
    // - Combines “before” and “after” parts by using the hash of the first part as the seed for the second part — works correctly and fast.
    // - Perfect for 64B header, extremely low latency.
    // skip_self: if true, skips the checksum field itself (to avoid circular dependency)
    [[nodiscard]] static inline uint64_t compute_checksum(const Header& header, bool skip_self = true) noexcept {
        const auto* bytes = reinterpret_cast<const uint8_t*>(&header);
        if (!skip_self) return XXH64(bytes, sizeof(Header), 0);

        constexpr size_t checksum_offset = offsetof(Header, checksum_le);
        constexpr size_t checksum_size = sizeof(uint64_t);
        // Hash bytes before checksum
        uint64_t hash1 = XXH64(bytes, checksum_offset, 0);
        // Hash bytes after checksum
        return XXH64(bytes + checksum_offset + checksum_size, sizeof(Header) - checksum_offset - checksum_size, hash1);
    }

    [[nodiscard]] inline bool validate_data() const noexcept {
        if (magic() != WAL_MAGIC) {
            WK_TRACE("[!!] Invalid WAL magic: expected " << std::hex << WAL_MAGIC << ", found " << magic() << std::dec);
            return false;
        }
        if (version() != WAL_VERSION) {
            WK_TRACE("[!!] Invalid WAL version: expected " << WAL_VERSION << ", found " << static_cast<int>(version()));
            return false;
        }
        if (header_size() != sizeof(Header)) {
            WK_TRACE("[!!] Invalid WAL header size: expected " << sizeof(Header) << ", found " << static_cast<int>(header_size()));
            return false;
        }

        return true;
    }

    [[nodiscard]] Status validate_checksum() const noexcept {
        // Compute checksum using the canonical little-endian layout
        uint64_t computed = compute_checksum(*this);
        if (checksum() != computed) {
            WK_TRACE("[!!] Segment header checksum mismatch: expected " << checksum() << ", computed " << computed);
            return Status::HEADER_CHECKSUM_MISMATCH;
        }
        return Status::OK;
    }

    // Full block validation (structural + checksums)
    [[nodiscard]] inline Status verify() const noexcept {
        // Step 1: validate header checksum
        Status status = validate_checksum();
        if (status != Status::OK) return status;
        // Step 2: structural validation
        if (!validate_data()) return Status::SEGMENT_POSSIBLY_CORRUPTED;

        return Status::OK;
    }

    inline void finalize(uint64_t chained) noexcept {
        set_closed_ts_ns(monotonic_clock::instance().now_ns());
        set_last_chained_checksum(chained);   // cross-segment anchor
        set_checksum(compute_checksum(*this));
    }

    inline void serialize(void* dest) const noexcept {
        std::memcpy(static_cast<uint8_t*>(dest), this, sizeof(Header));
    }

    inline void deserialize(const void* src) noexcept {
        std::memcpy(this, static_cast<const uint8_t*>(src), sizeof(Header));
    }

    inline uint32_t segment_size() const noexcept {
        return sizeof(Header) + (block_count() * sizeof(segment::Block));
    }
};

// ======================================================
// Layout validation (prevent ABI drift)
// ======================================================
static_assert(sizeof(Header) == 64, "Header must be exactly 64 bytes");
static_assert(alignof(Header) == 64, "Header must align to 64 bytes");
// sanity checks for offsets (compile-time guarantees)
static_assert(offsetof(Header, magic_le) == 0, "offset magic");
static_assert(offsetof(Header, version_le) == 2, "offset version");
static_assert(offsetof(Header, header_size_le) == 3, "offset header_size");
static_assert(offsetof(Header, segment_index_le) == 4, "offset segment_index");
static_assert(offsetof(Header, block_count_le) == 8, "offset block_count");
static_assert(offsetof(Header, event_count_le) == 12, "offset event_count");
static_assert(offsetof(Header, first_event_id_le) == 16, "offset first_event_id");
static_assert(offsetof(Header, last_event_id_le) == 24, "offset last_event_id");
static_assert(offsetof(Header, created_ts_ns_le) == 32, "offset created_ts_ns");
static_assert(offsetof(Header, closed_ts_ns_le) == 40, "offset closed_ts_ns");
static_assert(offsetof(Header, checksum_le) == 48, "offset checksum");
static_assert(offsetof(Header, last_chained_checksum_le) == 56, "offset last_chained_checksum");
// Ensure POD semantics
static_assert(std::is_standard_layout_v<Header> && "Header must have standard layout");
static_assert(std::is_trivially_copyable_v<Header> && "Header must be trivially copyable");


} // namespace segment
} // namespace wal
} // namespace flashstrike

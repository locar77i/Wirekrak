#pragma once

#include <string>
#include <cstdint>
#include <cstddef>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "flashstrike/wal/types.hpp"
#include "flashstrike/wal/segment/header.hpp"
#include "flashstrike/wal/segment/block.hpp"
#include "lcr/log/logger.hpp"


namespace flashstrike {
namespace wal {


struct WalSegmentInfo {
    std::string filepath;
    Status status;
    wal::segment::Header header{};
};


// Compose segment filename with fixed-width numeric index:
// Key notes: ultra-low latency, fully stack-allocated, branchless version:
// - No thread-local memory → safer in multi-threaded WAL workers.
// - No std::string allocations → purely stack-based, ultra-predictable.
// - No loops for prefix → only iterates over prefix if present (cheap).
// - Deterministic runtime → always O(width + prefix_len + 4) operations.
inline char* compose_segment_filename(char* buf, const char* prefix, size_t segment_index, int width = 8) noexcept {
    char* p = buf;
    // Copy prefix if present
    if (prefix && *prefix) {
        while (*prefix) *p++ = *prefix++;
        *p++ = '.';
    }
    // Fill fixed-width digits (branchless, right-to-left)
    size_t value = segment_index;
    for (int i = width - 1; i >= 0; --i) {
        p[i] = '0' + (value % 10);
        value /= 10;
    }
    p += width;
    // Append ".wal"
    *p++ = '.';
    *p++ = 'w';
    *p++ = 'a';
    *p++ = 'l';
    *p = '\0';
    return buf;
}


// Validate the entire WAL segment (all blocks + chained checksum)
[[nodiscard]] inline Status verify_full_segment_integrity(void* mmap_ptr, size_t valid_data_size, wal::segment::Header& segment_header) noexcept {
    //WK_TRACE("Verifying full WAL segment integrity...");
    if (!mmap_ptr) [[unlikely]] return Status::OPEN_FAILED;
    // Read and verify existing segment header
    segment_header.deserialize(mmap_ptr);
    Status status = segment_header.verify();
    WK_TRACE("WAL segment header: first_event_id=" << segment_header.first_event_id() << ", last_event_id=" << segment_header.last_event_id() << ", event_count=" << segment_header.event_count() << ", block_count=" << segment_header.block_count());
    if (status != Status::OK) [[unlikely]] return status;
    // Iterate over all blocks and verify them
    WK_TRACE("Walking all WAL blocks for full integrity check...");
    size_t offset = sizeof(wal::segment::Header);
    uint64_t prev_chained = 0;
    const uint8_t* base = reinterpret_cast<const uint8_t*>(mmap_ptr);
    for (size_t i=0; i < segment_header.block_count(); ++i) {
        if (offset + sizeof(wal::segment::Block) > valid_data_size) [[unlikely]] {
            WK_TRACE("[!!] WAL segment truncated: expected block " << i << " at offset " << offset << ", but valid data size is only " << valid_data_size);
            return Status::SEGMENT_CORRUPTED;
        }
        const wal::segment::Block* blk = reinterpret_cast<const wal::segment::Block*>(base + offset);
        //WK_TRACE(" -> Scanning WAL block at index " << i << ": event_count=" << blk->header.event_count() << " first_event_id=" << blk->header.first_event_id() << " last_event_id=" << blk->header.last_event_id() << " block_index=" << blk->header.block_index());
            
        // Full validation
        Status status = blk->verify(prev_chained);
        if (status != Status::OK) [[unlikely]] return status;
        // Store the previous chained checksum for next block and move forward
        prev_chained = blk->header.chained_checksum();
        offset += sizeof(wal::segment::Block);
    }
    // Cross-check with header last_chained_checksum
    if (prev_chained != segment_header.last_chained_checksum()) [[unlikely]] {
        return Status::SEGMENT_CORRUPTED;
    }
    return Status::OK;
}


// Read only the WAL header with minimal overhead
[[nodiscard]] static inline Status read_segment_header(const std::string& filepath, wal::segment::Header &out_header) {
    // Open the segment file
    int fd = ::open(filepath.c_str(), O_RDONLY
#if defined(O_CLOEXEC)
                    | O_CLOEXEC
#endif
    );
    if (fd < 0) return Status::OPEN_FAILED;
    // Check file size
    struct stat st{};
    if (::fstat(fd, &st) != 0 || static_cast<size_t>(st.st_size) < sizeof(wal::segment::Header)) {
        ::close(fd);
        return Status::OPEN_FAILED;
    }
    // Read header
    ssize_t n = ::pread(fd, &out_header, sizeof(wal::segment::Header), 0);
    ::close(fd);
    if (n != static_cast<ssize_t>(sizeof(wal::segment::Header))) return Status::READ_FAILED;
    // Return the result of validation
    return out_header.verify();
}

} // namespace wal
} // namespace flashstrike

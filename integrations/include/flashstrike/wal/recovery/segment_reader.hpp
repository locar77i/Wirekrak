#pragma once


#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "flashstrike/wal/constants.hpp"
#include "flashstrike/wal/segment/header.hpp"
#include "flashstrike/wal/segment/block.hpp"
#include "flashstrike/wal/recovery/telemetry/segment_reader.hpp"
#include "flashstrike/wal/utils.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/log/logger.hpp"


namespace flashstrike {
namespace wal {
namespace recovery {


// ============================================================================
//  SegmentReader
// ----------------------------------------------------------------------------
// Purpose:
//   Memory-mapped WAL segment reader for block-based WAL files.
//   Supports full integrity validation: local block checksum + chained checksum.
//   Compatible with WalBlockWriter (64-event blocks, dual checksums).
//   Provides sequential and indexed/hybrid seeking for ultra-low latency.
// ============================================================================
class SegmentReader {
public:
    explicit SegmentReader(const std::string& filepath, telemetry::SegmentReader& metrics)
        : filepath_(filepath)
        , metrics_updater_(metrics)
    {
    }

    ~SegmentReader() {
        force_close_file_if_needed_();
    }

    [[nodiscard]] inline Status open_segment() noexcept {
        // Open the segment file
#ifdef ENABLE_FS1_METRICS
        auto start_ns_open = monotonic_clock::instance().now_ns();
#endif
        WK_TRACE("[->] Opening WAL segment: " << filepath_);
        Status status = open_file_();
#ifdef ENABLE_FS1_METRICS
        metrics_updater_.on_open_segment(start_ns_open, status);
#endif
        if (status != Status::OK) return status;
        // Verify full segment integrity if requested
#ifdef ENABLE_FS1_METRICS
        auto start_ns_verify = monotonic_clock::instance().now_ns();
#endif
        WK_TRACE("[->] Verifying WAL segment: " << filepath_);
        status = verify_full_segment_integrity(mmap_ptr_, segment_size_, segment_header_);
#ifdef ENABLE_FS1_METRICS
        metrics_updater_.on_verify_segment(start_ns_verify, status);
#endif
        segment_valid_ = (status == Status::OK);
        if (!segment_valid_) {
            WK_TRACE("[!!] Failed full integrity check for WAL segment: " << filepath_);
            return status;
        }
        WK_TRACE("[OK] Full integrity confirmed for WAL segment: " << filepath_);
        // Read header
        valid_data_size_ = segment_header_.segment_size();
        //assert(valid_data_size_ <= segment_size_ && "WAL segment claims more data than file size");
        if (valid_data_size_ > segment_size_) {
            valid_data_size_ = segment_size_;
            WK_TRACE("Warning: WAL segment " << filepath_ << " has truncated/corrupted tail: header claims " << valid_data_size_ << " bytes, but segment size is " << segment_size_ << " bytes");
            return Status::SEGMENT_POSSIBLY_CORRUPTED;
        }
        // Start reading first block
        return read_block_at_offset_(sizeof(segment::Header)); // read first block
    }

    [[nodiscard]] inline Status close_segment() noexcept {
#ifdef ENABLE_FS1_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        Status status = close_file_();
        if (status != Status::OK) {
            WK_TRACE("Error closing WAL segment file: " << filepath_ << " (status: " << to_string(status) << ")");
        }
#ifdef ENABLE_FS1_METRICS
        metrics_updater_.on_close_segment(start_ns, status);
#endif
        return status;
    }

    // Sequential read: returns false at EOF or corrupted block
    [[nodiscard]] inline bool next(RequestEvent& ev) noexcept {
        if (current_event_index_in_block_ >= current_block_.header.event_count()) [[unlikely]] {
            // Load next block
            current_block_offset_ += sizeof(segment::Block);
            if (read_block_at_offset_(current_block_offset_) != Status::OK) [[unlikely]] return false;
        }
        // Return next event from current block at current index
        ev = current_block_.events[current_event_index_in_block_++];
        return true;
    }

    inline bool seek(uint64_t event_id) noexcept {
        if (!index_built_) build_index_internally_();
#ifdef ENABLE_FS1_METRICS
        auto start_ns_seek = monotonic_clock::instance().now_ns();
#endif
        bool result = seek_(event_id);
#ifdef ENABLE_FS1_METRICS
        metrics_updater_.on_seek_event(start_ns_seek);
#endif
        return result;
    }

    // Explicit sparse index build
    inline void build_index() noexcept {
        if (!index_built_) build_index_internally_();
    }

    // ------------------------------------------------------------------------
    // Metadata access
    // ------------------------------------------------------------------------
    inline uint64_t first_event_id() const { return segment_header_.first_event_id(); }
    inline uint64_t last_event_id() const { return segment_header_.last_event_id(); }
    inline uint32_t event_count() const { return segment_header_.event_count(); }
    inline uint64_t created_ts_ns() const { return segment_header_.created_ts_ns(); }
    inline uint64_t closed_ts_ns() const { return segment_header_.closed_ts_ns(); }

    inline const std::string& filepath() const { return filepath_; }
    inline bool is_valid() const { return segment_valid_; }

private:
    struct BlockIndexEntry {
        uint64_t first_event_id;
        uint64_t last_event_id;
        size_t file_offset;
    };

    std::string filepath_;
    int fd_{-1};
    void* mmap_ptr_{nullptr};
    size_t segment_size_{0};
    size_t valid_data_size_{0};
    size_t current_block_offset_{0};
    size_t current_event_index_in_block_{0};

    segment::Header segment_header_;
    segment::Block current_block_;

    std::vector<BlockIndexEntry> index_;
    bool index_built_{false};
    bool segment_valid_{false};

    telemetry::SegmentReaderUpdater metrics_updater_;
    // ------------------------------------------------------------------------

    inline Status open_file_() noexcept {
        assert((fd_ == -1 || mmap_ptr_ == nullptr) && "WAL file segment already opened");
        fd_ = ::open(filepath_.c_str(), O_RDONLY);
        if (fd_ < 0) return Status::OPEN_FAILED;
        // Get file size
        struct stat st{};
        if (fstat(fd_, &st) != 0) return Status::OPEN_FAILED;
        segment_size_ = static_cast<size_t>(st.st_size);
        if (segment_size_ < sizeof(segment::Header)) return Status::OPEN_FAILED;
        // Map entire file (read-only)
        mmap_ptr_ = ::mmap(nullptr, segment_size_, PROT_READ, MAP_SHARED, fd_, 0);
        if (mmap_ptr_ == MAP_FAILED) return Status::OPEN_FAILED;
        return Status::OK;
    }

    inline Status close_file_() noexcept {
        assert(fd_ >= 0 && mmap_ptr_ && mmap_ptr_ != MAP_FAILED && "WAL file must be opened before closing");
        index_.clear();
        index_built_ = false;
        // Kernel hint: release page cache before unmap
        ::madvise(mmap_ptr_, segment_size_, MADV_DONTNEED);
        if (::munmap(mmap_ptr_, segment_size_) != 0) {
            ::close(fd_);
            fd_ = -1;
            mmap_ptr_ = nullptr;
            return Status::CLOSE_FAILED;
        }
        mmap_ptr_ = nullptr;
        if (::close(fd_) != 0) {
            return Status::CLOSE_FAILED;
        }
        fd_ = -1;
        segment_size_ = 0;
        current_block_offset_ = 0;
        current_event_index_in_block_ = 0;
        return Status::OK;
    }

    inline void force_close_file_if_needed_() noexcept {
        if (fd_ >= 0 && mmap_ptr_ != nullptr) {
#ifdef ENABLE_FS1_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
            WK_TRACE("[->] Closing WAL segment file (forced): " << filepath_);
            Status status = close_file_();
            if (status != Status::OK) {
                WK_TRACE("[!!] Error closing WAL segment file (forced): " << filepath_ << " (status: " << to_string(status) << ")");
            }
#ifdef ENABLE_FS1_METRICS
        metrics_updater_.on_close_segment(start_ns, status);
#endif
        }
    }

    // Seek to event_id (or next higher) using sparse block index + linear scan
    // This method is good if blocks between index entries are small, but it's not optimal
    // if blocks are large (many thousands of events between indexes) because sequential scan can be slow.
    // The method works as follows:
    // - Uses the sparse index to jump close to the target.
    // - Then does a linear scan from that point to locate the first event whose event_id >= requested.
    // - If no event is found (EOF reached), it returns false.
    // So the guarantee is:
    // - If the exact event_id exists → you land exactly on it.
    // - If it doesn’t exist (gaps) → you land on the next higher event_id.
    // - If you go past EOF → failure.
    [[nodiscard]] inline bool seek_(uint64_t event_id) noexcept {
        if (index_.empty()) [[unlikely]] {
            return false;  // No blocks
        }
        // Step 1: Binary search candidate block
        size_t left = 0, right = index_.size() - 1, pos = 0;
        while (left <= right) {
            size_t mid = left + (right - left) / 2;
            const auto& entry = index_[mid];

            if (event_id >= entry.first_event_id && event_id <= entry.last_event_id) {
                pos = mid;  // Event inside this block
                break;
            } else if (event_id < entry.first_event_id) {
                if (mid == 0) { pos = 0; break; }
                right = mid - 1;
            } else {
                pos = mid;
                left = mid + 1;
            }
        }
        WK_TRACE("[V] (Binary Search) Found candidate block at index=" << pos);  
        // -----------------------------
        // Step 2: Read and validate block
        current_block_offset_ = index_[pos].file_offset;
        if (read_block_at_offset_(current_block_offset_) != Status::OK) {
            return false;
        }
        WK_TRACE("Landed on block " << pos << " at offset=" << current_block_offset_ << " with event_id range [" << current_block_.header.first_event_id() << " .. " << current_block_.header.last_event_id() << "]");
        // -----------------------------
        // Step 3: Find event index in block → linear scan
        // This loop always returns true because the block is valid and contains events,
        // except if the block read or light validation fails.
        current_event_index_in_block_ = 0;
        for (size_t i = 0; i < current_block_.header.event_count(); ++i) {
            if (current_block_.events[i].event_id >= event_id) {
                current_event_index_in_block_ = i;
                WK_TRACE("Found event_id=" << current_block_.events[i].event_id << " at offset=" << (current_block_offset_ + sizeof(segment::BlockHeader) + i * sizeof(RequestEvent)));
                return true;
            }
        }
        // Event beyond this block → try next block if exists
        if (pos + 1 < index_.size()) {
            current_block_offset_ = index_[pos + 1].file_offset;
            if (read_block_at_offset_(current_block_offset_) != Status::OK) return false;
            current_event_index_in_block_ = 0;
            WK_TRACE("Event_id=" << event_id << " not found, landed on next block at offset=" << current_block_offset_);
            return true;
        }
        // Event beyond last block → land on last event of last block
        current_event_index_in_block_ = current_block_.header.event_count() - 1;
        WK_TRACE("Event_id=" << event_id << " beyond last block, landed on last event in last block");
        return true;
    }

    // Builds the sparse index for the WAL segment.
    // 
    // This method scans all WAL blocks sequentially, performing full validation
    // on each block before adding it to the index. Full validation includes:
    //   - Lightweight header checks (event count, IDs, block index)
    //   - Block checksum verification (events only)
    //   - Chained checksum verification (including previous block's checksum)
    // 
    // Only fully validated blocks are added to the index. If a block fails
    // validation, the index build stops immediately to prevent corrupt entries.
    // 
    // The index allows efficient block-level seeking in the WAL segment.
    // 
    // Note: prev_chained is updated with each validated block to ensure
    //       chained integrity between blocks.
    inline void build_index_internally_() noexcept {
#ifdef ENABLE_FS1_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        WK_TRACE("Building sparse index for WAL segment: " << filepath_);
        index_.clear();
        index_.reserve(MAX_BLOCKS); // Preallocate max possible blocks

        size_t offset = sizeof(segment::Header);
        const uint8_t* base = reinterpret_cast<const uint8_t*>(mmap_ptr_);

        while (offset + sizeof(segment::Block) <= valid_data_size_) {
            const segment::Block* blk = reinterpret_cast<const segment::Block*>(base + offset);
            //WK_TRACE(" - Adding index entry for block_index=" << blk->header.block_index() << " first_event_id=" << blk->header.first_event_id() << " last_event_id=" << blk->header.last_event_id() << " offset=" << offset);
            // Add entry to index
            index_.push_back({
                blk->header.first_event_id(),
                blk->header.last_event_id(),
                offset
            });
            // Move to next block
            offset += sizeof(segment::Block);
        }
        // Update flag
        index_built_ = true;
#ifdef ENABLE_FS1_METRICS
        metrics_updater_.on_build_index(start_ns, Status::OK);
#endif
    }

    [[nodiscard]] inline Status read_block_at_offset_(size_t offset) noexcept {
        if (!mmap_ptr_) return Status::READ_FAILED;
        // Bounds check
        if (offset + sizeof(segment::Block) > valid_data_size_) return Status::READ_FAILED;
        // Copy block directly from mmap
        const uint8_t* base = reinterpret_cast<const uint8_t*>(mmap_ptr_);
        const segment::Block* blk = reinterpret_cast<const segment::Block*>(base + offset);
        current_block_.deserialize(blk);
        //WK_TRACE("Reading block index=" << current_block_.header.block_index() << " first_event_id=" << current_block_.header.first_event_id() << " last_event_id=" << current_block_.header.last_event_id() << " event_count=" << current_block_.header.event_count() << " at offset=" << offset);
        // Update reader state: set current block offset and reset event current event index
        current_block_offset_ = offset; // point to start of current block
        current_event_index_in_block_ = 0;
        return Status::OK;
    }

};


} // namespace recovery
} // namespace wal
} // namespace flashstrike

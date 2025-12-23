#pragma once


// 
#include <string>
#include <cstddef>
#include <cstdint>
#include <atomic>
#include <algorithm>
#include <cassert>
#include <filesystem>
// POSIX / Linux
#include <fcntl.h> 
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

// Project headers
#include "flashstrike/events.hpp"
#include "flashstrike/constants.hpp"
#include "flashstrike/wal/types.hpp"
#include "flashstrike/wal/constants.hpp"
#include "flashstrike/wal/segment/header.hpp"
#include "flashstrike/wal/segment/block.hpp"
#include "flashstrike/wal/recorder/telemetry/segment_writer.hpp"
#include "flashstrike/wal/utils.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/log/Logger.hpp"


namespace flashstrike {
namespace wal {
namespace recorder {

// ============================================================================
//  class SegmentWriter
//  ----------------------------------------------------------------------------
//  Manages a single WAL (Write-Ahead Log) segment file, handling low-level
//  event appends, block management, and persistence to disk.
//
//  Responsibilities:
//  -----------------
//      • Open new or existing WAL segment files with memory-mapped I/O.
//      • Append events to in-memory blocks, finalizing blocks when full.
//      • Flush partially filled blocks when needed (e.g., before rotation).
//      • Maintain segment header state, including first/last event IDs and
//        total event count.
//      • Finalize and sync segment to disk safely on close.
//      • Remove empty or invalid segments if necessary.
//
//  File Layout:
//  ------------
//      • WAL segments consist of a header (`wal::segment::Header`) followed by
//        multiple fixed-size blocks (`wal::segment::Block`), each containing multiple events.
//      • Block size and segment size are bounded by MIN_BLOCKS and MAX_BLOCKS.
//      • Event count per block is defined by `WAL_BLOCK_EVENTS`.
//
// Thread Safety:
// --------------
//  • `SegmentWriter` is not internally thread-safe for concurrent calls.
//  • Hot-path operations (`append`) assume exclusive access to the segment.
//  • Safe usage pattern:
//      - A single thread performs appends (`append()`).
//      - Ownership of a segment can be safely transferred to another thread
//        (e.g., via SPSC ring buffer) for opening/closing/persistence.
//      - Concurrent calls from multiple threads on the same object are not allowed.
//
//  Performance Notes:
//  ------------------
//      • Uses memory-mapped I/O for low-latency writes.
//      • Block writes are batched to reduce disk I/O overhead.
//      • Header and block updates maintain a running checksum chain for
//        data integrity.
//      • Flushes and syncs are minimized and can be forced when closing.
//
//  Usage Context:
//  --------------
//      • Typically used by a WAL manager to sequentially write events to
//        persistent storage.
//      • Call `open_new_segment()` or `open_existing_segment()` to initialize
//        before appending events.
//      • Call `append()` for each event; `flush_partial()` before segment
//        rotation, and `close_segment()` when done.
//      • Internal metrics can be collected if `ENABLE_FS1_METRICS` is defined.
//
//  Invariants:
//  -----------
//      • `segment_size_` = sizeof(wal::segment::Header) + num_blocks * sizeof(wal::segment::Block)
//      • `bytes_written_` always tracks the actual written bytes in memory-mapped file.
//      • All blocks are finalized with a chained checksum before writing.
// ----------------------------------------------------------------------------
class SegmentWriter {
public:
    SegmentWriter(const std::string& dir, const std::string& filename, size_t num_blocks, telemetry::SegmentWriter& metrics)
        : filepath_(dir + "/" + filename)
        , metrics_updater_(metrics)
    {
        // Clamp num_blocks to reasonable limits
        num_blocks_ = std::clamp(num_blocks, MIN_BLOCKS, MAX_BLOCKS);
        msync_threshold_ = num_blocks_ / 2; // sync after half the blocks
        segment_size_ = sizeof(wal::segment::Header) + num_blocks_ * sizeof(wal::segment::Block);
    }

    SegmentWriter(const std::string& filepath, size_t num_blocks, telemetry::SegmentWriter& metrics)
        : filepath_(filepath)
        , metrics_updater_(metrics)
    {
        // Clamp num_blocks to reasonable limits
        num_blocks_ = std::clamp(num_blocks, MIN_BLOCKS, MAX_BLOCKS);
        segment_size_ = sizeof(wal::segment::Header) + num_blocks_ * sizeof(wal::segment::Block);
    }

    ~SegmentWriter() {
        force_close_or_remove_if_needed_();
    }

    [[nodiscard]] inline Status open_new_segment(uint32_t segment_index) noexcept {
#ifdef ENABLE_FS1_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        Status status = open_new_file_(segment_index);
        if (status != Status::OK) [[likely]] {
            ::madvise(mmap_ptr_, segment_size_, MADV_WILLNEED); // pre-fault pages
        }
#ifdef ENABLE_FS1_METRICS
        metrics_updater_.on_open_new_segment(start_ns, status);
#endif
        return status;
    }

    [[nodiscard]] inline Status open_existing_segment() noexcept {
#ifdef ENABLE_FS1_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        Status status = open_existing_file_();
        if (status != Status::OK) [[likely]] {
            ::madvise(mmap_ptr_, segment_size_, MADV_WILLNEED); // pre-fault pages
        }
#ifdef ENABLE_FS1_METRICS
        metrics_updater_.on_open_existing_segment(start_ns, status);
#endif
        return status;
    }

    [[nodiscard]] inline Status close_segment(bool sync = false) noexcept {
#ifdef ENABLE_FS1_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        Status status = close_file_(sync);
#ifdef ENABLE_FS1_METRICS
        metrics_updater_.on_close_segment(start_ns, status);
#endif
        return status;
    }

    inline void touch() noexcept { // Prefault pages
        assert(fd_ >= 0 && mmap_ptr_ != nullptr && "WAL file must be opened before touching pages");
        // Touch one byte per page to trigger allocation
        const size_t page_size = static_cast<size_t>(::sysconf(_SC_PAGESIZE));
        volatile char* p = static_cast<volatile char*>(mmap_ptr_);
        for (size_t i = 0; i < segment_size_; i += page_size) {
            p[i] = 0;
        }
    }

    // Append a single event. When the block fills, it is finalized & written.
    [[nodiscard]] inline Status append(const RequestEvent& ev) noexcept {
        assert(fd_ >= 0 && mmap_ptr_ != nullptr && "WAL file must be opened before appending");
        // Add event to current block and update block header state
        //WK_DEBUG(" -> Appending event_id=" << ev.event_id << " to block index " << block_index_ << " (event_count=" << block_.header.event_count() << ")");
        if (block_.header.event_count() == 0) {
            block_.header.set_first_event_id(ev.event_id);
        }
        block_.header.set_last_event_id(ev.event_id);
        block_.events[block_.header.event_count()] = ev;
        block_.header.set_event_count(block_.header.event_count() + 1);
        // Update segment header
        if (segment_header_.first_event_id() == INVALID_EVENT_ID)
            segment_header_.set_first_event_id(ev.event_id);
        segment_header_.set_last_event_id(ev.event_id);
        segment_header_.set_event_count(segment_header_.event_count() + 1);
        // When block is full, write it
        if (block_.header.event_count() == WAL_BLOCK_EVENTS) {
            Status status = write_block_();
            return status;
        }
        //WK_DEBUG(" -> Header updated: first_event_id=" << segment_header_.first_event_id() << " last_event_id=" << segment_header_.last_event_id() << " event_count=" << segment_header_.event_count());
        return Status::OK;
    }

    // Flush partially filled block (e.g., before segment rotation)
    [[nodiscard]] inline Status flush_partial() noexcept {
        assert(fd_ >= 0 && mmap_ptr_ != nullptr && "WAL file must be opened before flushing block");
        if (block_.header.event_count() > 0)
            return write_block_();
        return Status::OK;
    }

    [[nodiscard]] inline Status flush(bool hard) {
        assert(fd_ >= 0 && mmap_ptr_ != nullptr && "WAL file must be opened before async flushing");
        if (hard) { // Wait until kernel confirms the write is on media
            if (::fdatasync(fd_) != 0)
                return Status::FSYNC_FAILED;
        } else { // Just schedule async flush to kernel
            if (::msync(mmap_ptr_, bytes_written_, MS_ASYNC) != 0)
                return Status::MSYNC_FAILED;
        }
        return Status::OK;
    }

    // Expose header info
    inline uint64_t first_event_id() const noexcept { return segment_header_.first_event_id(); }
    inline uint64_t last_event_id() const noexcept { return segment_header_.last_event_id(); }
    inline uint32_t event_count() const noexcept { return segment_header_.event_count(); }
    inline const std::string& filepath() const noexcept { return filepath_; }
    inline size_t segment_index() const noexcept { return segment_header_.segment_index(); }
    inline size_t bytes_written() const noexcept { return bytes_written_; }

    inline bool segment_is_full() const noexcept {
        return bytes_written() >= segment_size_;
    }

private:
    std::string filepath_;
    int fd_{-1};
    void* mmap_ptr_{nullptr};
    size_t num_blocks_{0};
    size_t msync_threshold_{0};
    size_t segment_size_{0};
    size_t bytes_written_{};
    wal::segment::Header segment_header_{};

    wal::segment::Block block_{};            // active block in memory
    uint32_t block_index_{0};     // next block index to write
    uint64_t prev_chained_{0};    // running checksum chain

    telemetry::SegmentWriterUpdater metrics_updater_;

    // ---------------------------------------------------------------------
    // Internal helpers
    // ---------------------------------------------------------------------

    [[nodiscard]] inline Status open_new_file_(uint32_t segment_index) noexcept {
        assert((fd_ == -1 || mmap_ptr_ == nullptr) && "WAL file segment already opened");
        WK_DEBUG("Opening WAL segment file: " << filepath_ << " (index " << segment_index << ")");
        if (std::filesystem::exists(filepath_)) {
            WK_DEBUG("Error: WAL segment file already exists, refusing to overwrite: " << filepath_);
            return Status::FILE_ALREADY_EXISTS;
        }
        fd_ = ::open(filepath_.c_str(), O_RDWR | O_CREAT | O_EXCL, 0644);
        if (fd_ < 0) return Status::OPEN_FAILED;
        //WK_DEBUG("Preallocating WAL segment file to " << segment_size_ << " bytes");
        if (::ftruncate(fd_, segment_size_) != 0) {
            return Status::OPEN_FAILED;
        }
        //WK_DEBUG("Mapping WAL segment file into memory");
        mmap_ptr_ = ::mmap(nullptr, segment_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (mmap_ptr_ == MAP_FAILED) {
            return Status::OPEN_FAILED; // "mmap failed"
        }
        WK_DEBUG("Writing initial WAL header");
        segment_header_.reset();
        segment_header_.set_magic(WAL_MAGIC);
        segment_header_.set_version(WAL_VERSION);
        segment_header_.set_header_size(sizeof(wal::segment::Header));
        segment_header_.set_segment_index(segment_index);
        segment_header_.set_first_event_id(INVALID_EVENT_ID);
        segment_header_.set_last_event_id(INVALID_EVENT_ID);
        segment_header_.set_created_ts_ns(monotonic_clock::instance().now_ns());
        // Write header to mmap area
        segment_header_.serialize(mmap_ptr_); // write to mmap area
        // Update state
        bytes_written_ = sizeof(wal::segment::Header);
        WK_DEBUG("Initialized WAL segment header (index " << segment_header_.segment_index() << "): first_event_id=" << segment_header_.first_event_id() << ", last_event_id=" << segment_header_.last_event_id() << ", event_count=" << segment_header_.event_count() << ", bytes_written=" << bytes_written_);
        return Status::OK;
    }

    [[nodiscard]] inline Status open_existing_file_() noexcept {
        assert((fd_ == -1 || mmap_ptr_ == nullptr) && "WAL file segment already opened");
        WK_DEBUG("Opening existing WAL segment file: " << filepath_);
        fd_ = ::open(filepath_.c_str(), O_RDWR);
        if (fd_ < 0) return Status::OPEN_FAILED;
        //WK_DEBUG("Getting file size");
        struct stat st{};
        if (::fstat(fd_, &st) != 0) {
            ::close(fd_);
            fd_ = -1;
            return Status::OPEN_FAILED;
        }
        if (static_cast<size_t>(st.st_size) < sizeof(wal::segment::Header)) {
            ::close(fd_);
            fd_ = -1;
            return Status::OPEN_FAILED; // File too small to contain a valid header
        }
        segment_size_ = static_cast<size_t>(st.st_size);
        if (segment_size_ < sizeof(wal::segment::Header)) return Status::OPEN_FAILED;
        //WK_DEBUG("Mapping WAL segment file into memory, size=" << segment_size_);
        mmap_ptr_ = ::mmap(nullptr, segment_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (mmap_ptr_ == MAP_FAILED) {
            ::close(fd_);
            fd_ = -1;
            return Status::OPEN_FAILED;
        }
        // Full integrity check
        Status status = verify_full_segment_integrity(mmap_ptr_, segment_size_, segment_header_);
        if (status != Status::OK) {
            WK_DEBUG("[!!] Failed full integrity check for WAL segment: " << filepath_);
#ifdef ENABLE_FS1_METRICS
            metrics_updater_.on_integrity_failure(status);
#endif
            return status;
        }
        WK_DEBUG("[OK] Full integrity confirmed for WAL segment: " << filepath_);
        bytes_written_ = restore_append_position_();
        WK_DEBUG("Loaded WAL segment header (index " << segment_header_.segment_index() << "): first_event_id=" << segment_header_.first_event_id() << ", last_event_id=" << segment_header_.last_event_id() << ", event_count=" << segment_header_.event_count() << ", bytes_written=" << bytes_written_);
        return Status::OK;
    }

    [[nodiscard]] inline Status close_file_(bool sync) noexcept {
        assert(fd_ >= 0 && mmap_ptr_ != nullptr && "WAL file must be opened before closing");
        WK_DEBUG("Closing WAL segment file: " << filepath_ << " (index " << segment_header_.segment_index() << ")");
        Status status = flush_partial();
        if (status != Status::OK) {
            WK_DEBUG("Error flushing final block before finalizing WAL segment: " << to_string(status));
        }
        status = finalize_segment_header_();
        if (status != Status::OK) {
            WK_DEBUG("Error finalizing WAL segment (header not written): " << to_string(status) << " (file: " << filepath_ << ")");
        }
        // 1. Ensure durability before any eviction
        if (sync && status == Status::OK && ::fdatasync(fd_) != 0) { // lightweight sync
            WK_DEBUG("Error syncing WAL segment file: " << filepath_ << " (index " << segment_header_.segment_index() << ")");
            ::close(fd_);
            fd_ = -1;
            return Status::FSYNC_FAILED;
        }
        // Kernel hint: release page cache before unmap
        // 2. Kernel hint: The mmap’d memory can be discarded
        ::madvise(mmap_ptr_, segment_size_, MADV_DONTNEED);
        // 3. Kernel hint: The kernel can drop the file’s cached pages
        posix_fadvise(fd_, 0, 0, POSIX_FADV_DONTNEED); // drop from cache
        if (::munmap(mmap_ptr_, segment_size_) != 0) {
            WK_DEBUG("Error unmapping WAL segment file: " << filepath_ << " (index " << segment_header_.segment_index() << ")");
            ::close(fd_);
            fd_ = -1;
            mmap_ptr_ = nullptr;
            return Status::CLOSE_FAILED;
        }
        mmap_ptr_ = nullptr;
        if (::close(fd_) != 0) {
            WK_DEBUG("Error closing WAL segment file: " << filepath_ << " (index " << segment_header_.segment_index() << ")");
            return Status::CLOSE_FAILED;
        }
        fd_ = -1;
        return status;
    }

    inline bool force_close_or_remove_if_needed_() noexcept {
        // Check if the file is open
        if (fd_ >= 0 && mmap_ptr_ != nullptr) {
            bool valid_segment = (segment_header_.last_event_id() != INVALID_EVENT_ID && bytes_written_ > 0);
            // Check if segment is empty / invalid
            if (valid_segment) {
                WK_DEBUG("[->] Force-closing valid WAL segment: " << filepath_);
#ifdef ENABLE_FS1_METRICS
                auto start_ns = monotonic_clock::instance().now_ns();
#endif
                // File is valid, just close it
                WK_DEBUG("[->] Closing WAL segment (forced): " << filepath_);
                Status status = close_file_(true); // sync on close
                if (status != Status::OK) {
                    WK_DEBUG("[!!] Error closing WAL segment (forced): " << filepath_ << " (status: " << to_string(status) << ")");
                }
#ifdef ENABLE_FS1_METRICS
                metrics_updater_.on_close_segment(start_ns, status);
#endif
                return status == Status::OK;
            }
            else {
                WK_DEBUG("[->] Force-closing no valid WAL segment: " << filepath_);
                // Close file descriptor if open
                if (fd_ >= 0) {
                    ::close(fd_);
                    fd_ = -1;
                }

                // Release the mmap if allocated
                if (mmap_ptr_ != nullptr) {
                    ::munmap(mmap_ptr_, segment_size_);
                    mmap_ptr_ = nullptr;
                }

                // Remove the file immediately using unlink
                if (!filepath_.empty()) {
                    int ret = ::unlink(filepath_.c_str());
                    if (ret == 0) {
                        WK_DEBUG("[OK] Removed WAL segment: " << filepath_);
                    } else {
                        WK_DEBUG("[!!] Failed to remove WAL segment: " << filepath_ << " (errno: " << errno << ")");
                    }
                }
                return true; // File was empty/invalid and removed
            }
        }
        return false; // File already closed
    }

    inline size_t restore_append_position_() noexcept {
        size_t bytes_written = sizeof(wal::segment::Header);
        block_index_ = 0;
        prev_chained_  = 0;
        WK_DEBUG("Walk existing blocks to find true end-of-data position");
        const uint8_t* base = reinterpret_cast<const uint8_t*>(mmap_ptr_);
        const wal::segment::Block* blk = nullptr;
        for (size_t i=0; i < segment_header_.block_count(); ++i) {
            if (bytes_written + sizeof(wal::segment::Block) > segment_size_) [[unlikely]] {
                WK_DEBUG("[!!] WAL segment truncated: expected block " << i << " at offset " << bytes_written << ", but valid data size is only " << segment_size_);
                break;
            }
            blk = reinterpret_cast<const wal::segment::Block*>(base + bytes_written);
            assert(reinterpret_cast<uintptr_t>(blk) % alignof(wal::segment::Block) == 0 && "Unaligned WAL block");
            //WK_DEBUG(" -> Scanning WAL block at index " << i << ": event_count=" << blk->header.event_count() << " first_event_id=" << blk->header.first_event_id() << " last_event_id=" << blk->header.last_event_id() << " block_index=" << blk->header.block_index());
            // Stop if block is not full (last block)
            if (blk->header.event_count() < WAL_BLOCK_EVENTS) {
                WK_DEBUG(" -> Stopping WAL block scan at block index " << block_index_ << " due to partial block (event_count=" << blk->header.event_count() << ")");
                break;
            }
            prev_chained_ = blk->header.chained_checksum();
            bytes_written += sizeof(wal::segment::Block);
            ++block_index_;
        }
        if (blk != nullptr && blk->header.event_count() < WAL_BLOCK_EVENTS) {
            block_.deserialize(blk);
            WK_DEBUG("Restored WAL append position: block_index=" << block_index_ << " event_index_in_block=" << (blk->header.event_count() - 1) << " bytes_written=" << bytes_written);

        }
        return bytes_written;
    }

    [[nodiscard]] inline Status write_block_() noexcept {
#ifdef ENABLE_FS1_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        block_.finalize(block_index_, prev_chained_);
        // Write block directly into mapped region
        uint8_t* base = reinterpret_cast<uint8_t*>(mmap_ptr_);
        block_.serialize(base + bytes_written_); // write to mmap area
        bytes_written_ += sizeof(wal::segment::Block);
        // Update running checksum chain, block index and block header state
        prev_chained_ = block_.header.chained_checksum();
        segment_header_.set_block_count(++block_index_);
        block_.reset();
        Status status = Status::OK;
        /*
        if (msync_threshold_ && msync_threshold_ && segment_header_.block_count()) {
           // hint to kernel to start writing dirty pages.
           Status status = async_flush_(); // TODO: consider removing this async flush if too much overhead and rely on persistence worker only
        }
        */
#ifdef ENABLE_FS1_METRICS
        metrics_updater_.on_write_block_(start_ns);
#endif
        return status;
    }

    [[nodiscard]] inline Status async_flush_() noexcept { // Hey kernel, please start writing these dirty pages soon.
        assert(fd_ >= 0 && mmap_ptr_ != nullptr && "WAL file must be opened before async flushing");
        if (::msync(mmap_ptr_, bytes_written_, MS_ASYNC) != 0) {
            WK_DEBUG("[!!] WAL async flush failed for file: " << filepath_);
            return Status::MSYNC_FAILED;
        }
        return Status::OK;
    }

    [[nodiscard]] inline Status finalize_segment_header_() noexcept {
        if (fd_ < 0) return Status::OPEN_FAILED;
        WK_DEBUG("Finalizing WAL segment " << segment_header_.segment_index() << ": total events=" << segment_header_.event_count());
        segment_header_.finalize(prev_chained_);
        WK_DEBUG("Updating WAL header: first_event_id=" << segment_header_.first_event_id() << " last_event_id=" << segment_header_.last_event_id() << " event_count=" << segment_header_.event_count() << " checksum=" << segment_header_.checksum() << " last_chained_checksum=" << segment_header_.last_chained_checksum());
        // Write updated header
        ssize_t n = ::pwrite(fd_, &segment_header_, sizeof(wal::segment::Header), 0);
        if (n != sizeof(wal::segment::Header)) return Status::WRITE_FAILED;
        return Status::OK;
    }

};


} // namespace recorder
} // namespace wal
} // namespace flashstrike

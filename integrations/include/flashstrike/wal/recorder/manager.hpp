#pragma once

// Standard Library
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <filesystem>
#include <system_error>
#include <cassert>
#include <cstdint>
#include <utility>
#include <thread>
#include <chrono>

// Project headers
#include "flashstrike/wal/types.hpp"
#include "flashstrike/wal/constants.hpp"
#include "flashstrike/wal/segment/header.hpp"
#include "flashstrike/wal/segment/block.hpp"
#include "flashstrike/wal/recorder/meta.hpp"
#include "flashstrike/wal/recorder/segment_writer.hpp"
#include "flashstrike/wal/recorder/worker/segment_preparer.hpp"
#include "flashstrike/wal/recorder/worker/segment_maintainer.hpp"
#include "flashstrike/wal/recorder/worker/meta_coordinator.hpp"
#include "flashstrike/wal/recorder/telemetry.hpp"
#include "flashstrike/wal/utils.hpp"
#include "lcr/local/ring.hpp"
#include "lcr/lockfree/spmc_task_ring.hpp"
#include "lcr/system/cpu_relax.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/log/Logger.hpp"


namespace flashstrike {
namespace wal {
namespace recorder {

// ============================================================================
//  class Manager
//  ----------------------------------------------------------------------------
//  High-level manager for Write-Ahead Log (WAL) writing, segment rotation, and
//  background persistence. Orchestrates WAL writers, segment preparation, and
//  durability/retention mechanisms to provide low-latency, lock-free event appends.
//
//  Responsibilities:
//  -----------------
//      • Maintain the active WAL segment for fast, in-memory appends.
//      • Rotate WAL segments when they reach size limits.
//      • Coordinate with `worker::SegmentPreparer` to pre-create WAL segments asynchronously.
//      • Transfer completed WAL segments to `worker::SegmentMaintainer` for durable
//        closure, compression, and retention enforcement.
//      • Maintain WAL metadata via `MetaWorker`, including last segment index,
//        last offset, and last appended event ID.
//      • Scan existing WAL and compressed segments at startup for recovery.
//
//  Hot Path (Append):
//  -----------------
//      • `append(const RequestEvent&)` is lock-free, allocation-free, and suitable
//        for high-throughput event ingestion.
//      • Rotates segments transparently when the current segment is full.
//      • Updates in-memory WAL metadata for subsequent persistence.
//
//  Segment Lifecycle Management:
//  -----------------------------
//      1. Active Segment: currently being appended to in memory.
//      2. Prepared Segment: asynchronously created by `worker::SegmentPreparer`.
//      3. Written Segment: handed off to `worker::SegmentMaintainer` for durable closure.
//      4. Compressed Segment: archived to LZ4 format to enforce retention policies.
//
//  Recovery & Initialization:
//  ---------------------------
//      • On startup, scans the WAL directory for existing `.wal` and `.lz4` files.
//      • Loads `MetaState` from metadata file or attempts recovery from
//        the last valid WAL segment if metadata is missing.
//      • Ensures the next segment index aligns with the last known state.
//
//  Thread Safety:
//  --------------
//      • Orchestrates multiple background workers: `worker::SegmentPreparer`, 
//        `worker::SegmentMaintainer`, and `MetaWorker`.
//      • Main append path is lock-free; only background workers perform blocking I/O.
//      • Uses atomic counters and a ring buffer for safe inter-thread communication.
//
//  Performance Notes:
//  ------------------
//      • Hot path append is optimized for low latency with minimal overhead.
//      • Segment preparation and persistence happen off the critical path.
//      • Spin-wait with `cpu_relax()` ensures non-blocking push to ring buffers.
//      • Segment rotation and flushing are batched to minimize I/O overhead.
//
//  Usage Context:
//  --------------
//      • Construct with WAL directory, segment block size, and retention limits.
//      • Call `initialize()` to launch background workers and prepare the first segment.
//      • Call `append()` in the hot path for event ingestion.
//      • Call `shutdown()` to gracefully stop all workers, persist the current segment,
//        and flush metadata.
//
//  Invariants:
//  -----------
//      • `num_blocks_` is clamped within `MIN_BLOCKS` and `MAX_BLOCKS`.
//      • `segment_size_` corresponds to the size of a single WAL segment.
//      • `meta_state_` always reflects the last appended event and offset.
//      • Background workers ensure hot and cold segment retention limits are enforced.
// ----------------------------------------------------------------------------
class Manager {
public:
    Manager(const std::string& dir, size_t num_blocks, size_t max_segments, size_t max_compressed_segments, Telemetry& metrics)
        : wal_dir_(dir)
        , num_blocks_(std::clamp(num_blocks, MIN_BLOCKS, MAX_BLOCKS))
        , segment_size_(sizeof(wal::segment::Header) + num_blocks_ * sizeof(wal::segment::Block))
        , writer_(nullptr)
        , segment_preparer_(dir, num_blocks, metrics.segment_preparer_metrics, metrics.segment_writer_metrics)
        , segments_to_persist_()
        , segments_to_freeze_()
        , segments_to_free_()
        , maintainer_worker_(dir, max_segments, max_compressed_segments, segments_to_persist_, segments_to_freeze_, segments_to_free_, metrics.segment_maintainer_metrics)
        , meta_coordinator_(dir, "wal_meta.dat", metrics.meta_store_metrics)
        , segment_writer_metrics_(metrics.segment_writer_metrics)
        , metrics_updater_(metrics.manager_metrics)
    {
        std::filesystem::create_directory(wal_dir_);
    }

    ~Manager() {
    }

    // Initialize the background worker and fetch first ready segment
    [[nodiscard]] inline Status initialize() noexcept {
        //WK_DEBUG("worker::SegmentMaintainer - WAL segment size set to " << segment_size_ << " bytes (" << num_blocks_ << " blocks of " << WAL_BLOCK_EVENTS << " events each)");
        // Ensure directory exists (best-effort)
        std::filesystem::create_directories(wal_dir_);
        // Scan existing segments and compressed segments at startup: populate ring buffer and compressed list.
        Status status = scan_segments_();
        if (status != Status::OK) {
            WK_DEBUG("[!!] WAL Writer Manager failed to scan existing segments: " << to_string(status));
            return status;
        }
        // Restore the last active segment (or create new if none)
        status = restore_or_create_active_segment_();
        if (status != Status::OK) { // background thread: compress / retention
            WK_DEBUG("[!!] WAL Writer Worker failed to initialize properly: " << to_string(status));
            return status;
        }
        // Set the internal ring buffer of wal files and clear the scanned list of wal files
        if(wals_.size() > wal_files_.capacity()) {
            WK_DEBUG("[!!] Warning: number of scanned WAL files (" << wals_.size() << ") exceeds internal ring buffer size (" << wal_files_.capacity() << "). Some files will be ignored.");
        }
        for (const auto& wal : wals_) {
            if (!wal_files_.full()) wal_files_.push(wal);
        }
        wals_.clear();
        // Set the internal ring buffer of lz4 files and clear the scanned list of lz4 files
        if (lz4s_.size() > lz4_files_.capacity()) {
            WK_DEBUG("[!!] Warning: number of scanned LZ4 files (" << lz4s_.size() << ") exceeds internal ring buffer size (" << lz4_files_.capacity() << "). Some files will be ignored.");
        }
        for (const auto& lz4 : lz4s_) {
            if (!lz4_files_.full()) lz4_files_.push(lz4);
        }
        lz4s_.clear();

        segment_preparer_.start(meta_state_.last_segment_index + 1); // Prepare next segments beginning from last + 1
        maintainer_worker_.start();
        meta_coordinator_.start();
        return Status::OK;
    }

    inline void shutdown() noexcept {
        persist_current_segment_();
        maintainer_worker_.stop();
        meta_coordinator_.update(meta_state_); // flush final meta
        meta_coordinator_.stop();
        segment_preparer_.stop();
    }

    // Hot path append: lock-free, no allocations
    [[nodiscard]] inline Status append(const RequestEvent& ev) noexcept {
#ifdef ENABLE_FS1_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        assert(writer_ != nullptr && "WAL writer must be initialized");
        // Rotate if current segment exceeds size
        //WK_DEBUG("Append event_id=" << ev.event_id << ", current segment size=" << writer_->bytes_written() << "/" << segment_size_ << " bytes");
        Status status;
        if (writer_->segment_is_full()) {
            WK_DEBUG("Rotating WAL segment due to size limit");
            status = rotate_segment_();
            if (status != Status::OK) {
                WK_DEBUG("[!!] Failed to rotate WAL segment: " << to_string(status));
                return status;
            }
        }
        // Append event to current writer
        status = writer_->append(ev);
        meta_state_.last_event_id = ev.event_id;
        meta_state_.last_offset = writer_->bytes_written();
#ifdef ENABLE_FS1_METRICS
        metrics_updater_.on_append_event(start_ns, status);
#endif
        return status;
    }

    inline MetaState get_meta_state() const noexcept { return meta_coordinator_.get_state(); }

private:
    std::string wal_dir_;
    size_t num_blocks_;
    size_t segment_size_;

    std::shared_ptr<SegmentWriter> writer_{};
    worker::SegmentPreparer segment_preparer_;

    lcr::lockfree::spmc_task_ring<std::shared_ptr<SegmentWriter>, WAL_PERSIST_RING_BUFFER_SIZE> segments_to_persist_{};
    lcr::lockfree::spmc_task_ring<std::string, WAL_HOT_RING_BUFFER_SIZE> segments_to_freeze_{};
    lcr::lockfree::spmc_task_ring<std::string, WAL_COLD_RING_BUFFER_SIZE> segments_to_free_{};

    worker::SegmentMaintainer maintainer_worker_;

    worker::MetaCoordinator meta_coordinator_;
    MetaState meta_state_{};

    std::vector<std::string> wals_;
    std::vector<std::string> lz4s_;

    lcr::local::ring<std::string, WAL_HOT_RING_BUFFER_SIZE> wal_files_{};

    lcr::local::ring<std::string, WAL_COLD_RING_BUFFER_SIZE> lz4_files_{};

    telemetry::SegmentWriter& segment_writer_metrics_;
    telemetry::ManagerUpdater metrics_updater_;

    // ---------------------------------------------------------------------------------------------------------------------
    // Internal helpers
    // ---------------------------------------------------------------------------------------------------------------------

    // Scan existing files on startup.
    // We push .wal files into the ring (spinning if ring is full), and .lz4 into cold list.
    [[nodiscard]] inline Status scan_segments_() noexcept {
        // Check if directory exists
        if (!std::filesystem::exists(wal_dir_) || !std::filesystem::is_directory(wal_dir_)) {
            WK_DEBUG("[!!] WAL directory does not exist or is not a directory: " << wal_dir_);
            return Status::DIRECTORY_NOT_FOUND;
        }
        for (auto& entry : std::filesystem::directory_iterator(wal_dir_)) {
            if (!entry.is_regular_file()) continue;
            auto path = entry.path();
            if (path.extension() == ".wal") wals_.push_back(path.string());
            else if (path.extension() == ".lz4") lz4s_.push_back(path.string());
        }
        std::sort(wals_.begin(), wals_.end());
        std::sort(lz4s_.begin(), lz4s_.end());
        WK_DEBUG("[SCAN] Found " << wals_.size() << " hot segments (*.wal files) and " << lz4s_.size() << " cold segments (*.lz4 files) on dir: " << wal_dir_);
        return Status::OK;
    }

    [[nodiscard]] inline Status restore_or_create_active_segment_() noexcept {
#ifdef ENABLE_FS1_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif // #ifdef ENABLE_FS1_METRICS
        // Load WAL meta (last segment index and offset)
        bool state_recovered = recover_last_state_(meta_state_);
        Status status;
        if (state_recovered) {
            status = prepare_first_segment_from_scanned_();
            if (status != Status::OK) {
                WK_DEBUG("[!!] Impossible to recover last WAL segment, starting from scratch.");
                status = prepare_first_segment_from_scratch_();
            }
        }
        else {
            status = prepare_first_segment_from_scratch_();
        }
#ifdef ENABLE_FS1_METRICS
        metrics_updater_.on_init_active_segment(start_ns, status);
#endif // #ifdef ENABLE_FS1_METRICS
        return status;
    }

    // Attempt to recover the last state using WAL meta first, else recover from last segments. Key Points:
    // - WAL meta preferred: fast load from small metadata file.
    // - If not found/corrupted/invalid/obsolete, then fallback to recover last valid segment header.
    // Returns true if state recovery succeeded, else false (start fresh).
    [[nodiscard]] bool recover_last_state_(MetaState& meta_state) noexcept {
        bool meta_loaded = meta_coordinator_.load();
        if(meta_loaded) {
            meta_state = meta_coordinator_.get_state();
            WK_DEBUG("[WAL] Meta state loaded: last_segment_index=" << meta_state.last_segment_index << ", last_offset=" << meta_state.last_offset << ", last_event_id=" << meta_state.last_event_id);
            char segment_name[3 + 8 + 4 + 1]; // "FS" + 8 digits + ".wal" + null
            compose_segment_filename(segment_name, "FS", meta_state.last_segment_index, 8);
            // Check that the segment file name is the newest in the scanned list
            std::string segment_filepath = wal_dir_ + "/" + segment_name;
            //WK_DEBUG("WAL meta indicates last segment file: " << segment_filepath << " (scanned last segment file: " << (wals_.empty() ? "N/A" : wals_.back()) << ")");
            if (wals_.empty() || wals_.back() == segment_filepath) {
                return true;
            }
        }
        if (recover_last_state_when_meta_is_missing_(meta_state)) { // Successfully recovered last state
            WK_DEBUG("[WAL] State recovered: last_segment_index=" << meta_state.last_segment_index << ", last_offset=" << meta_state.last_offset << ", last_event_id=" << meta_state.last_event_id);
            return true;
        }
        WK_DEBUG("[WAL] No WAL meta and unable to recover last state (starting fresh)");
        return false;
    }

    // Attempt to recover the last segment when WAL meta is missing/corrupted. Key Points:
    // - Backwards iteration: Reads newest segments first (wal_ is sorted).
    // - Minimal I/O: Stops immediately when a valid header is found.
    // - Ultra-low latency friendly: Only reads as many headers as necessary, avoids scanning all files if the last few are valid.
    // - Safe against corruption: Skips corrupted or partially written segments gracefully.
    // Returns true if recovery succeeded, false if we must create a new segment.
    [[nodiscard]] bool recover_last_state_when_meta_is_missing_(MetaState& meta_state) noexcept {
        // Iterate backwards (newest first) until we find a valid header
        for (size_t i = wals_.size(); i-- > 0; ) {
            const auto& filepath = wals_[i];
            WK_DEBUG("Attempting to read WAL header from " << filepath);
            wal::segment::Header hdr{};
            Status status = read_segment_header(filepath, hdr);
            if (status == Status::OK) {
                // Valid header found, compute next write offset
                meta_state.last_offset = hdr.segment_size();
                meta_state.last_event_id = hdr.last_event_id();
                meta_state.last_segment_index = hdr.segment_index();
                WK_DEBUG("[OK] Recovered WAL meta state from last segment: " << filepath << ", segment_index=" << meta_state.last_segment_index << ", segment_offset=" << meta_state.last_offset << ", last_event_id=" << meta_state.last_event_id);
                return true;
            } else {
                WK_DEBUG("[!!] Failed to read WAL header " << filepath << " (" << to_string(status) << ")");
                pop_last_scanned_segment_();
            }
        }
        return false;
    }

    [[nodiscard]] inline Status prepare_first_segment_from_scratch_() noexcept {
        meta_state_ = MetaState{}; // reset state
        size_t segment_index = meta_state_.last_segment_index;
        char segment_name[3 + 8 + 4 + 1]; // "FS" + 8 digits + ".wal" + null
        compose_segment_filename(segment_name, "FS", segment_index, 8);
        WK_DEBUG("Preparing the first WAL segment: " << segment_name << " (new file)");
        writer_ = std::make_unique<SegmentWriter>(wal_dir_, segment_name, num_blocks_, segment_writer_metrics_);
        auto status = writer_->open_new_segment(segment_index);
        if (status != Status::OK) {
            WK_DEBUG("Error creating new WAL segment file: " << to_string(status));
            writer_.reset();
            return status;
        }
        meta_coordinator_.update(meta_state_);
        return Status::OK;
    }

    [[nodiscard]] inline Status prepare_first_segment_from_scanned_() noexcept {
        for (size_t i = wals_.size(); i-- > 0; ) {
            const auto& filepath = wals_[i];
            WK_DEBUG("Preparing the first WAL segment: " << filepath << " (existing file)");
            writer_ = std::make_unique<SegmentWriter>(filepath, num_blocks_, segment_writer_metrics_);
            auto status = writer_->open_existing_segment();
            if (status == Status::OK) { // Successfully opened existing segment
                WK_DEBUG("[OK] Opened existing WAL segment file: " << filepath);
                meta_state_.last_segment_index = writer_->segment_index();
                meta_state_.last_offset = writer_->bytes_written();
                meta_state_.last_event_id = writer_->last_event_id();
                meta_coordinator_.update(meta_state_);
                return Status::OK;
            } else {
                WK_DEBUG("[!!] Error opening existing WAL segment file: " << to_string(status));
                pop_last_scanned_segment_();
                writer_.reset();
            }
        }
        return Status::SEGMENT_NOT_FOUND;
    }


    inline Status pop_last_scanned_segment_() noexcept {
        // Delete wals_ entry since it's corrupted/unreadable and remove file from disk (best-effort)
        assert(!wals_.empty() && "There must be at least one scanned WAL segment to pop");
        const auto& filepath = wals_.back();
        Status status;
        std::error_code ec;
        std::filesystem::remove(filepath, ec);
        if (!ec) {
            WK_DEBUG("[OK] Deleted invalid/corrupted WAL segment file: " << filepath);
            status = Status::OK;
        } else {
            WK_DEBUG("[!!] Failed to delete invalid/corrupted WAL segment file: " << filepath << " (error: " << ec.message() << ")");
            status = Status::FILE_NOT_DELETED;
        }
        wals_.pop_back();
        return status;
    }


    [[nodiscard]] inline Status prepare_next_segment_() noexcept {
        writer_ = segment_preparer_.get_next_segment();
        if (!writer_) {
            WK_DEBUG("[!!] Error obtaining prepared WAL segment from preparer worker");
            return Status::SEGMENT_NOT_FOUND;
        }
        meta_state_.last_segment_index = writer_->segment_index();
        meta_coordinator_.update(meta_state_);
        return Status::OK;
    }

    [[nodiscard]] inline Status rotate_segment_() noexcept {
        assert(writer_ != nullptr && "WAL writer must be initialized before rotation");
#ifdef ENABLE_FS1_METRICS
        const auto start_ns = monotonic_clock::instance().now_ns();
#endif
        if (wal_files_.full()) {
            std::string oldest_segment;
            wal_files_.pop(oldest_segment);
            size_t spins = 0;
            while (!segments_to_free_.push(std::move(oldest_segment))) {
                if (++spins > SPINS_GUESS) {
                    spins = 0;
                    std::this_thread::yield();
                } else {
                    lcr::system::cpu_relax();
                }
            }
        }
        wal_files_.push(writer_->filepath());
#ifdef ENABLE_FS1_METRICS
        metrics_updater_.on_work_planning(start_ns);
#endif
        persist_current_segment_();
        Status status = prepare_next_segment_();

#ifdef ENABLE_FS1_METRICS
        metrics_updater_.on_segment_rotation(start_ns);
#endif
        return status;
    }

    inline void persist_current_segment_() noexcept {
        // Push current one to written queue
#ifdef ENABLE_FS1_METRICS
        const auto start_ns = monotonic_clock::instance().now_ns();
#endif
        // Transfer ownership to the worker ring
        size_t spins = 0;
        while (!segments_to_persist_.push(std::move(writer_))) {
            if (++spins > SPINS_GUESS) {
                spins = 0;
                std::this_thread::yield();
            } else {
                lcr::system::cpu_relax();
            }
        }
#ifdef ENABLE_FS1_METRICS
        metrics_updater_.on_persist_current_segment(start_ns);
#endif
    }

};


} // namespace recorder
} // namespace wal
} // namespace flashstrike

#pragma once

// Standard headers
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <algorithm>
#include <vector>
#include <fstream>
#include <filesystem>
#include <system_error>
#include <cassert>
#include <cstdint>
#include <utility>
// POSIX
#include <fcntl.h>
#include <unistd.h>

// LZ4 compression
#include <lz4.h>
#include <lz4frame.h>
#include <lz4hc.h>
#include <lz4frame_static.h>

// Project headers
#include "flashstrike/wal/types.hpp"
#include "flashstrike/wal/constants.hpp"
#include "flashstrike/wal/recorder/segment_writer.hpp"
#include "flashstrike/wal/recorder/telemetry/worker/segment_maintainer.hpp"
#include "lcr/lockfree/spmc_task_ring.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/log/Logger.hpp"


namespace flashstrike {
namespace wal {
namespace recorder {
namespace worker {


// ============================================================================
//  class SegmentMaintainer
//  ----------------------------------------------------------------------------
//  Background worker responsible for persisting completed WAL segments to disk,
//  enforcing retention policies, and compressing old segments to conserve storage.
//
//  Responsibilities:
//  -----------------
//      • Monitor the ring buffer of completed WAL segments (`segments_to_persist_`) 
//        produced by the WAL writer.
//      • Close WAL segment files durably, ensuring fsync before compression or deletion.
//      • Maintain hot (uncompressed) and cold (compressed) segment lists.
//      • Compress old hot segments to LZ4 format when exceeding `max_segments_`.
//      • Enforce retention policies for hot and cold segments to avoid disk bloat.
//      • Provide consistency checks for testing or monitoring via `verify_consistency()`.
//
//  Segment Lifecycle:
//  -----------------
//      1. Hot segment: newly written WAL segment, not yet compressed.
//      2. Cold segment: compressed or archived WAL segment.
//      3. Segments beyond retention limits are either compressed (hot → cold) or deleted.
//
//  Thread Safety:
//  --------------
//      • Operates entirely on a dedicated background thread (`worker_thread_`).
//      • Interacts with `segments_to_persist_` ring buffer in a thread-safe manner.
//      • Methods `start()` and `stop()` are thread-safe; other operations are internal.
//
//  Performance Notes:
//  ------------------
//      • Asynchronous persistence ensures main WAL append path is non-blocking.
//      • Exponential backoff sleep avoids busy spinning when idle.
//      • Compression and file deletion are performed off the hot path.
//      • Supports both LZ4 block-format and LZ4 frame-format compression.
//
//  Usage Context:
//  --------------
//      • Constructed with a target directory, hot/cold segment limits, and a reference
//        to the ring buffer of written segments.
//      • Call `start()` to launch the background thread; call `stop()` to terminate
//        and flush remaining work.
//      • Segment consistency can be validated via `verify_consistency()`.
//
//  Invariants:
//  -----------
//      • Number of hot segments ≤ `max_segments_`.
//      • Number of cold segments ≤ `max_compressed_segments_`.
//      • All WAL files in `hot_segments_` or `cold_segments_` are non-empty.
//      • Hot segments are durably persisted before compression.
//      • Background thread guarantees eventual closure and compression/deletion of
//        all segments.
// ----------------------------------------------------------------------------
class SegmentMaintainer {
public:
    SegmentMaintainer(const std::string& dir, size_t max_segments, size_t max_compressed_segments,
                    lcr::lockfree::spmc_task_ring<std::shared_ptr<SegmentWriter>, WAL_PERSIST_RING_BUFFER_SIZE>& segments_to_persist,
                    lcr::lockfree::spmc_task_ring<std::string, WAL_HOT_RING_BUFFER_SIZE>& segments_to_freeze,
                    lcr::lockfree::spmc_task_ring<std::string, WAL_COLD_RING_BUFFER_SIZE>& segments_to_free,
                    telemetry::worker::SegmentMaintainer& metrics)
        : wal_dir_(dir)
        , max_segments_(max_segments)
        , max_compressed_segments_(max_compressed_segments)
        , segments_to_persist_(segments_to_persist)
        , segments_to_freeze_(segments_to_freeze)
        , segments_to_free_(segments_to_free)
        , stop_worker_(false)
        , metrics_updater_(metrics)
    {
        // Clamp max segments to reasonable limits
        max_segments_ = std::clamp(max_segments, WAL_MIN_HOT_SEGMENTS, WAL_MAX_HOT_SEGMENTS);
        max_compressed_segments_ = std::clamp(max_compressed_segments, WAL_MIN_COLD_SEGMENTS, WAL_MAX_COLD_SEGMENTS);
#ifdef ENABLE_FS1_METRICS
        metrics_updater_.set_max_segments(max_segments_);
        metrics_updater_.set_max_compressed_segments(max_compressed_segments_);
#endif // #ifdef ENABLE_FS1_METRICS
    }

    ~SegmentMaintainer() {}

    inline void start() {
        WK_DEBUG("[->] Launching WAL Segment Maintainer thread...");
        worker_thread_ = std::thread([this]() { this->persistence_loop_(); });
    }

    inline void stop() {
        WK_DEBUG("[<-] Stopping WAL Segment Maintainer thread...");
        stop_worker_.store(true, std::memory_order_release);
        if (worker_thread_.joinable())
            worker_thread_.join();
        WK_DEBUG("[OK] WAL Segment Maintainer stopped.");
    }

/*
    // Verify consistency between memory state and directory files.
    // Intended for use in tests / monitoring only (not hot path).
    [[nodiscard]] inline Consistency verify_consistency() const noexcept {
        std::vector<std::string> wals;
        std::vector<std::string> lz4s;

        for (auto& entry : std::filesystem::directory_iterator(wal_dir_)) {
            if (!entry.is_regular_file()) continue;
            auto path = entry.path();
            if (path.extension() == ".wal") wals.push_back(path.string());
            else if (path.extension() == ".lz4") lz4s.push_back(path.string());
        }
        std::sort(wals.begin(), wals.end());
        std::sort(lz4s.begin(), lz4s.end());

        // --- 1. Hot segments retention ---
        if (wals.size() > max_segments_) {
            return Consistency::TooManyHotSegments;
        }

        // --- 2. Cold segments retention ---
        if (lz4s.size() > max_compressed_segments_) {
            return Consistency::TooManyColdSegments;
        }

        // --- 3. Hot segments retention ---
        if (lz4s.size() != cold_segments_.size()) {
            return Consistency::ColdListMismatch;
        }
        for (size_t i = 0; i < lz4s.size(); ++i) {
            if (lz4s[i] != cold_segments_[i]) {
                return Consistency::ColdListMismatch;
            }
        }

        // --- 4. Check that all files are non-empty ---
        for (auto& f : wals) {
            if (std::filesystem::file_size(f) == 0) return Consistency::EmptyFileDetected;
        }
        for (auto& f : lz4s) {
            if (std::filesystem::file_size(f) == 0) return Consistency::EmptyFileDetected;
        }

        return Consistency::Ok;
    }
*/

private:
    std::string wal_dir_;
    size_t max_segments_;
    size_t max_compressed_segments_;

    // Ring buffer provided by main WAL manager
    lcr::lockfree::spmc_task_ring<std::shared_ptr<SegmentWriter>, WAL_PERSIST_RING_BUFFER_SIZE>& segments_to_persist_;
    lcr::lockfree::spmc_task_ring<std::string, WAL_HOT_RING_BUFFER_SIZE>& segments_to_freeze_;
    lcr::lockfree::spmc_task_ring<std::string, WAL_COLD_RING_BUFFER_SIZE>& segments_to_free_;

    std::atomic<bool> stop_worker_;
    std::thread worker_thread_;

    telemetry::worker::SegmentMaintainerUpdater metrics_updater_;

    // ---------------------------------------------------------------------------------------------------------------------
    // Internal helpers
    // ---------------------------------------------------------------------------------------------------------------------

    inline void persistence_loop_() noexcept {
        static const std::chrono::milliseconds min_sleep(10);    // minimum sleep when under heavy load
        static const std::chrono::milliseconds max_sleep(1000);  // maximum sleep when idle
        std::chrono::milliseconds sleep_time = min_sleep;
        while (!stop_worker_.load(std::memory_order_acquire)) {
#ifdef ENABLE_FS1_METRICS
            auto start_ns = monotonic_clock::instance().now_ns();
#endif
            bool did_work = false;
            if (!segments_to_persist_.empty()) {
                did_work = true;
                persist_next_segment_();
            }
            if (!segments_to_freeze_.empty()) {
                did_work = true;
                freeze_next_segment_();
            }
            if (!segments_to_free_.empty()) {
                did_work = true;
                free_next_segment_();
            }
            //did_work |= enforce_hot_retention_();
            // Adjust sleep based on activity
            if (did_work) {
                sleep_time = min_sleep;  // reset to minimal sleep if work was done
            } else {
                sleep_time = std::min(sleep_time * 2, max_sleep); // exponential backoff
            }
            // Sleep briefly after processing all work
            std::this_thread::sleep_for(sleep_time);
#ifdef ENABLE_FS1_METRICS
            metrics_updater_.on_persistence_loop(did_work, start_ns, sleep_time.count());
#endif
        }
        // Final cleanup on exit: ensure hot retention and flush meta
        WK_DEBUG("[->]   Maintenance thread stopping — final close, enforcement and meta flush...");
        while (!segments_to_persist_.empty()) {
#ifdef ENABLE_FS1_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
            WK_DEBUG("[->] Finalizing remaining written WAL segment...");
            persist_next_segment_();
            //enforce_hot_retention_();
#ifdef ENABLE_FS1_METRICS
            metrics_updater_.on_persistence_loop(true, start_ns, 0);
#endif
        }
    }

    inline void persist_next_segment_() noexcept {
        std::shared_ptr<SegmentWriter> writer;
        if (segments_to_persist_.pop(writer)) {
            WK_DEBUG("[->] WAL segment written completed: " << writer->filepath() << ", bytes_written=" << writer->bytes_written());
            Status status = writer->close_segment(true); // sync=true: ensure durability before proceeding
            if(status != Status::OK) {
                WK_DEBUG("[!!] Error closing WAL segment file: " << to_string(status));
            }
            WK_DEBUG("[WAL] Segment closed: " << writer->filepath() << ", bytes_written=" << writer->bytes_written());
            assert(status == Status::OK && "Failed on closing WAL segment file");
            writer.reset();
        }
    }

    inline void freeze_next_segment_() noexcept {
#ifdef ENABLE_FS1_METRICS
            auto start_ns = monotonic_clock::instance().now_ns();
#endif
        std::string oldest_segment;
        if (segments_to_freeze_.pop(oldest_segment)) {
            WK_DEBUG("[->]   Enforcing cold retention: compressing oldest WAL segment: " << oldest_segment);
            // Ensure data flushed to disk before compressing (best-effort)
            segment_flush_(oldest_segment);
#ifdef ENABLE_FS1_METRICS
            auto compression_start_ns = monotonic_clock::instance().now_ns();
#endif
            //bool ok = compress_segment_lz4_block_format_(oldest_segment);
            bool ok = compress_segment_lz4_frame_format_(oldest_segment);
#ifdef ENABLE_FS1_METRICS
            metrics_updater_.on_hot_segment_compression(ok, compression_start_ns);
#endif    
            if (ok) {
                // Remove original uncompressed WAL
                std::error_code ec;
                std::filesystem::remove(oldest_segment, ec);
                if (ec) {
                    WK_DEBUG("[->]   [!!] Removal failed for original WAL segment: " << oldest_segment << " (error: " << ec.message() << ")");
                }
                else {
                    WK_DEBUG("[->]   [OK] Removed original WAL segment: " << oldest_segment);
                }
                ok = (!ec); // only OK if removal succeeded
                // Add compressed file to cold list
                //std::string compressed_name = oldest_segment + ".lz4";
            } else { // compression failed: keep original file in cold list as-is (or move aside)
                WK_DEBUG("[->]   [!!] Compression failed for WAL segment: " << oldest_segment << " (removing anyway)");
                std::error_code ec;
                std::filesystem::remove(oldest_segment, ec);
                if (ec) {
                    WK_DEBUG("[->]   [!!] Removal failed for original WAL segment: " << oldest_segment << " (error: " << ec.message() << ")");
                }
                else {
                    WK_DEBUG("[->]   [OK] Removed original WAL segment: " << oldest_segment);
                }
            }
#ifdef ENABLE_FS1_METRICS
            metrics_updater_.on_hot_segment_retention(ok, start_ns);
#endif  
        }
    }

    inline void free_next_segment_() noexcept {
#ifdef ENABLE_FS1_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        std::string oldest_segment;
        if (segments_to_free_.pop(oldest_segment)) {
            WK_DEBUG("[->]   Enforcing cold retention: removing oldest compressed WAL segment: " << oldest_segment);
            std::error_code ec;
            bool removed = std::filesystem::remove(oldest_segment, ec);
            if (ec || !removed) {
                WK_DEBUG("[->]   [!!] Deletion failed for old WAL segment: " << oldest_segment << " (error: " << ec.message() << ")");
            }
            else {
                WK_DEBUG("[->]   [OK] Deleted old compressed WAL segment: " << oldest_segment);
            }
#ifdef ENABLE_FS1_METRICS
            metrics_updater_.on_cold_segment_deletion(removed, start_ns);
#endif
        }
    }


    // Final confirmation of durability, done off the critical path so the main thread doesn’t block. This method ensures:
    // - The WAL segment that’s already closed is now durably persisted.
    // - Even if the system crashes right now, that segment is recoverable and replayable.
    inline void segment_flush_(const std::string& fname) const noexcept {
        WK_DEBUG("Async flush for WAL segment: " << fname);
         // Check file size first to avoid unnecessary open/fsync on empty files
        std::error_code ec;
        auto size = std::filesystem::file_size(fname, ec);
        if (ec || size == 0) {
            // File doesn't exist or is empty: nothing to flush
            return;
        }
        // Open, fsync, close
        int fd = ::open(fname.c_str(), O_WRONLY);
        if (fd >= 0) {
            ::fsync(fd); // blocking here is OK, old segment only
            WK_DEBUG("[OK] Flushed segment to disk: " << fname);
            ::close(fd);
        }
    }

    // Basic LZ4 block compression
    [[nodiscard]] inline bool compress_segment_lz4_block_format_(const std::string& fname) const noexcept {
        const std::string out_fname = fname + ".lz4";

        std::ifstream in(fname, std::ios::binary | std::ios::ate);
        if (!in) return false;
        WK_DEBUG("Opened WAL segment for compression: " << fname);
        auto size = in.tellg();
        in.seekg(0, std::ios::beg);

        std::vector<char> input_buffer(size);
        in.read(input_buffer.data(), size);
        in.close();

        // Allocate worst-case compressed size
        int max_dst_size = LZ4_compressBound(static_cast<int>(size));
        std::vector<char> compressed_buffer(max_dst_size);

        int compressed_size = LZ4_compress_default(
            input_buffer.data(),
            compressed_buffer.data(),
            static_cast<int>(size),
            max_dst_size
        );

        if (compressed_size <= 0) return false; // compression failed
        WK_DEBUG("Compression reduced size from " << size << " to " << compressed_size);
        std::ofstream out(out_fname, std::ios::binary);
        out.write(compressed_buffer.data(), compressed_size);

        // Explicitly close to catch flush/close errors
        out.close();
        if (!out) return false;
        WK_DEBUG("[OK] Compressed WAL segment: " << fname << " to " << out_fname << " (original size: " << size << ", compressed size: " << compressed_size << ")");
        return true;
    }

    // LZ4 frame compression
    [[nodiscard]] inline bool compress_segment_lz4_frame_format_(const std::string& fname) const noexcept {
        const std::string out_fname = fname + ".lz4";

        // Open input file
        std::ifstream in(fname, std::ios::binary);
        if (!in) return false;

        // Open output file
        std::ofstream out(out_fname, std::ios::binary);
        if (!out) return false;

        // LZ4 frame compression context
        LZ4F_errorCode_t err;
        LZ4F_preferences_t prefs;
        memset(&prefs, 0, sizeof(prefs));
        prefs.frameInfo.blockSizeID = LZ4F_max4MB; // max block size
        prefs.frameInfo.blockMode = LZ4F_blockLinked;
        prefs.frameInfo.contentChecksumFlag = LZ4F_noContentChecksum;

        LZ4F_compressionContext_t cctx;
        err = LZ4F_createCompressionContext(&cctx, LZ4F_VERSION);
        if (LZ4F_isError(err)) return false;

        // Begin compression frame
        std::vector<char> out_buf(LZ4F_HEADER_SIZE_MAX);
        size_t headerSize = LZ4F_compressBegin(cctx, out_buf.data(), out_buf.size(), &prefs);
        if (LZ4F_isError(headerSize)) {
            LZ4F_freeCompressionContext(cctx);
            return false;
        }
        out.write(out_buf.data(), headerSize);

        // Compress input file in chunks
        constexpr size_t CHUNK_SIZE = 1 << 20; // 1 MiB
        std::vector<char> in_buf(CHUNK_SIZE);
        std::vector<char> comp_buf(LZ4F_compressBound(CHUNK_SIZE, &prefs));

        while (in) {
            in.read(in_buf.data(), CHUNK_SIZE);
            std::streamsize read_bytes = in.gcount();
            if (read_bytes <= 0) break;

            size_t cSize = LZ4F_compressUpdate(cctx,
                                            comp_buf.data(), comp_buf.size(),
                                            in_buf.data(), read_bytes,
                                            nullptr);
            if (LZ4F_isError(cSize)) {
                LZ4F_freeCompressionContext(cctx);
                return false;
            }
            out.write(comp_buf.data(), cSize);
        }

        // Finish compression
        size_t cSize = LZ4F_compressEnd(cctx, comp_buf.data(), comp_buf.size(), nullptr);
        if (LZ4F_isError(cSize)) {
            LZ4F_freeCompressionContext(cctx);
            return false;
        }
        out.write(comp_buf.data(), cSize);

        LZ4F_freeCompressionContext(cctx);
        
        // Explicitly close to catch flush/close errors
        out.close();
        if (!out) return false;
        WK_DEBUG("[OK] Compressed WAL segment: " << fname << " to " << out_fname);
        return true;
    }

};


} // namespace worker
} // namespace recorder
} // namespace wal
} // namespace flashstrike

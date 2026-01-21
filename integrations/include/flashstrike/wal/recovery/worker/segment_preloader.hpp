#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

#include "flashstrike/wal/types.hpp"
#include "flashstrike/wal/constants.hpp"
#include "flashstrike/wal/recovery/segment_reader.hpp"
#include "flashstrike/wal/recovery/telemetry/worker/segment_preloader.hpp"
#include "flashstrike/wal/recovery/telemetry/segment_reader.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/system/cpu_relax.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/log/logger.hpp"


namespace flashstrike {
namespace wal {
namespace recovery {
namespace worker {

// -------------------------------------------------------------------------------
// SegmentPreloader
// -------------------------------------------------------------------------------
// Background I/O worker that handles two asynchronous tasks:
//  1. Preloading and verifying upcoming WAL segments to eliminate blocking I/O
//     during recovery playback.
//  2. Closing exhausted segments pushed into the finished_ring_ by the manager.
//
// Design overview:
//  • Launched by Manager after scan_segments_() completes.
//  • Receives a static list of “future” segments (after the starting one).
//  • Sequentially opens and fully verifies each segment in the background.
//  • Pushes ready-to-use SegmentReader instances into the prepared_ring_.
//
// Runtime behavior:
//  • Worker thread runs independently of the manager's replay loop.
//  • Spin-waits with cpu_relax() and occasional std::this_thread::yield() for
//    efficient low-latency ring buffer operations.
//  • Continuously checks finished_ring_ for exhausted segments and closes them
//    asynchronously to keep the manager hot path free from I/O.
//  • Stops automatically when all preloading is done and stop() is called.
//
// Interaction with Manager:
//  • Manager pops readers from prepared_ring_ when the current segment is exhausted.
//  • Manager pushes finished readers into finished_ring_ instead of closing them
//    directly.
//  • Worker ensures that segment closure does not interfere with event replay,
//    preserving deterministic latency.
//
// Performance considerations:
//  • Completely removes heavy mmap() + integrity verification + segment closure
//    from the manager's hot path.
//  • Typical improvement: 100–250 ms shaved off next-segment transition time.
//  • Ring buffer capacities tuned for bursty or large-segment workloads.
//  • SPSC model ensures zero locking and deterministic timing.
//
// Reliability:
//  • Skips invalid/corrupted segments gracefully without blocking the manager.
//  • Thread-safe shutdown via stop_requested_ flag and joinable thread handle.
// -------------------------------------------------------------------------------
class SegmentPreloader {
public:
    explicit SegmentPreloader(lcr::lockfree::spsc_ring<std::unique_ptr<SegmentReader>, MAX_PRELOADED_SEGMENTS>& prepared_ring,
                               lcr::lockfree::spsc_ring<std::unique_ptr<SegmentReader>, WAL_RING_BUFFER_SIZE>& finished_ring,
                               telemetry::worker::SegmentPreloader& metrics,
                               telemetry::SegmentReader& segment_reader_metrics)
        : prepared_ring_(prepared_ring)
        ,  finished_ring_(finished_ring)
        ,  stop_requested_(false)
        ,  done_(true) // initially done
        , metrics_updater_(metrics)
        , segment_reader_metrics_(segment_reader_metrics)
    {}

    ~SegmentPreloader() {
    }

    // Start the worker with a vector of segments (moved in)
    void start(std::vector<WalSegmentInfo> segments) noexcept {
        // Prevent starting if already running
        if (worker_thread_.joinable()) {
            WK_TRACE("[Worker] Already running!");
            return;
        }

        segments_ = std::move(segments);
        stop_requested_.store(false, std::memory_order_release);
        done_.store(false, std::memory_order_release);

        worker_thread_ = std::thread(&SegmentPreloader::run, this);
    }

    void stop() noexcept {
        stop_requested_.store(true, std::memory_order_release);
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    [[nodiscard]] bool preloading_is_done() const noexcept {
        return preloading_done_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool is_done() const noexcept {
        return done_.load(std::memory_order_acquire);
    }

private:
    void run() noexcept {
        WK_TRACE("[Worker] Starting WAL segment preloading (segments=" << segments_.size() << ")");

        const size_t total_segments = segments_.size();
        size_t next_index = 0;
        unsigned int spins = 0;

        while (!stop_requested_.load(std::memory_order_relaxed)) {
            bool did_work = false;

            // 1. Preload next segment if capacity allows
            if (next_index < total_segments && !prepared_ring_.full()) {
                const auto& seg = segments_[next_index++];
                if (seg.status != Status::OK) {
                    WK_TRACE("[Worker] Skipping invalid segment: " << seg.filepath);
                    continue;
                }
                // Create reader and open segment
                auto reader = std::make_unique<SegmentReader>(seg.filepath, segment_reader_metrics_);
                WK_TRACE("[Worker] Opening segment: " << seg.filepath);
#ifdef ENABLE_FS1_METRICS
                auto start_ns = monotonic_clock::instance().now_ns();
#endif
                Status status = reader->open_segment();
#ifdef ENABLE_FS1_METRICS
                metrics_updater_.on_preload_segment(start_ns, status);
#endif
                if (status != Status::OK) {
                    WK_TRACE("[Worker] Failed to open segment: " << seg.filepath << " (" << to_string(status) << ")");
                    continue;
                }
                // Try to push into the prepared ring
                spins = 0;
                while (!prepared_ring_.push(std::move(reader))) {
                    if (stop_requested_.load(std::memory_order_relaxed)) break;
                    if (++spins > SPINS_GUESS) {
                        spins = 0;
                        std::this_thread::yield();
                    } else {
                        lcr::system::cpu_relax();
                    }
                }
                WK_TRACE("[Worker] Segment ready: " << seg.filepath);
                did_work = true;
            }
            else if (next_index >= total_segments) {
                preloading_done_.store(true, std::memory_order_release);
            }
            // 2. Process finished segments asynchronously
            std::unique_ptr<SegmentReader> finished_seg;
            if (finished_ring_.pop(finished_seg)) {
                if (finished_seg) {
                    WK_TRACE("[Worker] Closing finished segment: " << finished_seg->filepath());
#ifdef ENABLE_FS1_METRICS
                    auto start_ns = monotonic_clock::instance().now_ns();
#endif
                    Status status = finished_seg->close_segment();
#ifdef ENABLE_FS1_METRICS
                    metrics_updater_.on_finish_segment(start_ns, status);
#endif
                    if (status != Status::OK) {
                        WK_TRACE("[Worker] Failed closing finished segment: " << finished_seg->filepath()
                            << " (" << to_string(status) << ")");
                    }
                }
                did_work = true;
            }
            // 3. If no work was done, relax CPU
            if (!did_work) {
                if (++spins > SPINS_GUESS) {
                    spins = 0;
                    std::this_thread::yield();
                } else {
                    lcr::system::cpu_relax();
                }
            }
        }
        // Signal done
        done_.store(true, std::memory_order_release);
        WK_TRACE("[Worker] Completed WAL segment preloading and cleanup");
    }

    // Members
    std::vector<WalSegmentInfo> segments_;                  // segments to preload
    lcr::lockfree::spsc_ring<std::unique_ptr<SegmentReader>, MAX_PRELOADED_SEGMENTS>& prepared_ring_; // worker -> manager
    lcr::lockfree::spsc_ring<std::unique_ptr<SegmentReader>, WAL_RING_BUFFER_SIZE>& finished_ring_; // manager -> worker
    std::thread worker_thread_;
    std::atomic<bool> stop_requested_;
    std::atomic<bool> preloading_done_;
    std::atomic<bool> done_;

    telemetry::worker::SegmentPreloaderUpdater metrics_updater_;
    telemetry::SegmentReader& segment_reader_metrics_;
};


} // namespace worker
} // namespace recovery
} // namespace wal
} // namespace flashstrike

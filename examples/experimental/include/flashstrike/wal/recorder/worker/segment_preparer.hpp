#pragma once

// Standard headers
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <utility>

// Project headers
#include "flashstrike/types.hpp"
#include "flashstrike/constants.hpp"
#include "flashstrike/wal/recorder/segment_writer.hpp"
#include "flashstrike/wal/recorder/telemetry/worker/segment_preparer.hpp"
#include "flashstrike/wal/recorder/telemetry/segment_writer.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/log/Logger.hpp"


namespace flashstrike {
namespace wal {
namespace recorder {
namespace worker {

// ============================================================================
//  class SegmentPreparer
//  ----------------------------------------------------------------------------
//  Background worker responsible for preparing new WAL segments ahead of
//  time to ensure low-latency writes in the main WAL append path.
//
//  Responsibilities:
//  -----------------
//      • Maintain a small queue of pre-allocated WAL segment writers.
//      • Create new WAL segment files asynchronously in a dedicated thread.
//      • Notify consumers when a new segment is ready for appending events.
//      • Ensure segments are fully initialized and ready for memory-mapped I/O.
//
//  Queue & Backpressure:
//  ---------------------
//      • Uses a fixed-size SPSC ring buffer with `PREPARE_QUEUE_CAPACITY` slots.
//      • If the queue is full, the worker yields the CPU until space is available.
//      • Consumers call `get_next_segment()` to retrieve prepared segments.
//
//  Thread Safety:
//  --------------
//      • `SegmentPreparer` internally manages a dedicated background thread.
//      • Thread-safe methods:
//          - `start()` / `stop()` — control the worker thread.
//          - `get_next_segment()` — blocks or waits for a prepared segment.
//      • The worker thread updates the segment queue and notifies via condition
//        variable. External access to the queue must go through `get_next_segment()`.
//
//  Performance Notes:
//  ------------------
//      • Asynchronous segment preparation avoids blocking the main WAL writer.
//      • Memory-mapped I/O initialization is performed off the hot path.
//      • Light-weight sleeps (`yield` or short `sleep_for`) prevent busy spinning.
//
//  Usage Context:
//  --------------
//      • Typically used by a WAL manager to ensure new segments are ready
//        before rotation or when current segment fills.
//      • Call `start(segment_index)` to begin preparation.
//      • Call `stop()` to terminate gracefully and drain the queue.
//      • Call `get_next_segment()` to fetch a pre-prepared WAL segment for writing.
//
//  Invariants:
//  -----------
//      • Queue never exceeds `PREPARE_QUEUE_CAPACITY` segments.
//      • Each segment is created with a unique, incrementing `segment_index_`.
//      • All WAL segment files prepared by this worker are ready for immediate
//        append operations upon retrieval.
// ----------------------------------------------------------------------------
class SegmentPreparer {
public:
    static constexpr size_t PREPARE_QUEUE_CAPACITY = 4;

    SegmentPreparer(const std::string& dir, size_t num_blocks, recorder::telemetry::worker::SegmentPreparer& metrics, recorder::telemetry::SegmentWriter& segment_metrics)
        : wal_dir_(dir)
        , num_blocks_(num_blocks)
        , segment_index_(0)
        , running_(false)
        , segment_writer_metrics_(segment_metrics)
        , metrics_updater_(metrics)
    {}

    ~SegmentPreparer() {
        stop();
    }

    inline void start(size_t segment_index) {
        WK_DEBUG("[->] Launching WAL Segment Preparer thread...");
        segment_index_ = segment_index;
        running_.store(true, std::memory_order_release);
        worker_ = std::thread(&SegmentPreparer::prepare_loop_, this);
    }

    inline void stop() {
        WK_DEBUG("[<-] Stopping WAL Segment Preparer thread...");
        running_.store(false, std::memory_order_release);
        cv_.notify_all();
        if (worker_.joinable())
            worker_.join();

        // Drain any unconsumed prepared segments
        std::shared_ptr<recorder::SegmentWriter> tmp;
        while (queue_.pop(tmp)) {
            tmp.reset();
        }
        WK_DEBUG("[OK] WAL Segment Preparer stopped and queue drained.");
    }

    [[nodiscard]] inline std::shared_ptr<recorder::SegmentWriter> get_next_segment() noexcept {
#ifdef ENABLE_FS1_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        std::shared_ptr<recorder::SegmentWriter> seg;
        while (running_.load(std::memory_order_acquire)) {
            // Try non-blocking pop first
            if (queue_.pop(seg)) {
#ifdef ENABLE_FS1_METRICS
                metrics_updater_.on_get_next_segment(start_ns);
#endif
                return seg;
            }
            // Otherwise block until notified or stopped
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [&] {
                return !running_.load(std::memory_order_acquire) || !queue_.empty();
            });
            // Loop back to try again (safe under spurious wakeups)
        }

        return nullptr; // gracefully stop
    }

private:
    std::string wal_dir_;
    size_t num_blocks_;
    size_t segment_index_;

    std::atomic<bool> running_;
    std::thread worker_;
    lcr::lockfree::spsc_ring<std::shared_ptr<recorder::SegmentWriter>, PREPARE_QUEUE_CAPACITY> queue_;

    std::mutex mutex_;
    std::condition_variable cv_;

    recorder::telemetry::SegmentWriter& segment_writer_metrics_;
    recorder::telemetry::worker::SegmentPreparerUpdater metrics_updater_;

    // Thread loop for preparing new WAL segments
    inline void prepare_loop_() {
        while (running_.load(std::memory_order_acquire)) {
            if (!queue_.full()) {
                // Prepare new segment
                size_t segment_index = segment_index_++;
                char segment_name[3 + 8 + 4 + 1]; // "FS" + 8 digits + ".wal" + null
                compose_segment_filename(segment_name, "FS", segment_index, 8);
                WK_DEBUG("[->] Preparing WAL segment index=" << segment_index << " name=" << segment_name << " (new file)");
                auto seg = std::make_shared<recorder::SegmentWriter>(wal_dir_, segment_name, num_blocks_, segment_writer_metrics_);
                // Open new segment file
                Status status = seg->open_new_segment(segment_index);
                if (status == Status::OK) {
                    seg->touch(); // prefault pages
                    if( queue_.push(std::move(seg))) {
                        cv_.notify_one();
                    }
                } else {
                    WK_DEBUG("[!!] Error creating WAL segment file (status: " << to_string(status) << ")");
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            } else {
                std::this_thread::yield(); // backpressure
            }
        }
    }
};


} // namespace worker
} // namespace recorder
} // namespace wal
} // namespace flashstrike

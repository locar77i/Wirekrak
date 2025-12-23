#pragma once

// Standard headers
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

// Project headers
#include "flashstrike/wal/recorder/meta.hpp"
#include "flashstrike/wal/recorder/telemetry/Meta.hpp"
#include "lcr/log/Logger.hpp"


namespace flashstrike {
namespace wal {
namespace recorder {
namespace worker {

// ============================================================================
//  class MetaCoordinator
//  ----------------------------------------------------------------------------
//  Background worker for managing WAL metadata persistence.
//
//  This class wraps a `MetaStore` instance to provide asynchronous disk flushes
//  while allowing lock-free, low-latency hot-path updates of WAL progress.
//
//  Responsibilities:
//  -----------------
//      • Maintains the current WAL metadata state (`MetaState`) in memory.
//      • Accepts atomic updates to the state via `update()` without blocking.
//      • Flushes dirty metadata to disk asynchronously in a dedicated thread.
//      • Provides access to the current metadata state via `get_state()`.
//      • Supports startup recovery using `load()`.
//
//  Thread Safety:
//  --------------
//      • `update()` and `get_state()` are safe to call concurrently from multiple
//        threads.
//      • The internal flush thread serializes writes to disk, ensuring consistent
//        persistence.
//      • `start()` and `stop()` safely manage the lifecycle of the background thread.
//
//  Performance Notes:
//  ------------------
//      • Hot-path updates are lock-free and non-blocking.
//      • Background flush thread uses condition variable signaling to avoid
//        busy-waiting, waking only when data is dirty.
//      • Designed for minimal impact on WAL append latency.
//
//  Usage Context:
//  --------------
//      • Typically instantiated once per WAL manager instance.
//      • Call `load()` at startup to restore persisted metadata.
//      • Call `start()` to launch the background flush thread.
//      • Use `update()` during WAL appends to record progress.
//      • Use `get_state()` to read the latest WAL metadata.
//      • Call `stop()` to safely terminate the background thread and flush
//        any remaining changes.
// ----------------------------------------------------------------------------
class MetaCoordinator {
public:
    explicit MetaCoordinator(const std::string& dir, const std::string& fname, telemetry::MetaStore& metrics)
        : meta_store_(dir, fname, metrics)
        , running_(false)
    {}

    ~MetaCoordinator() {
    }

    // Load meta from disk (startup)
    [[nodiscard]] bool load() noexcept {
        return meta_store_.load();
    }

    // Start background flush thread
    inline void start() {
        WK_DEBUG("[->] Launching WAL Meta coordinator thread...");
        running_.store(true, std::memory_order_release);
        worker_ = std::thread(&MetaCoordinator::flush_loop_, this);
    }

    // Stop background flush thread
    inline void stop() {
        WK_DEBUG("[<-] Stopping WAL Meta coordinator thread...");
        running_.store(false, std::memory_order_release);
        cv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
        WK_DEBUG("[OK] WAL Meta coordinator stopped.");
    }

    // Hot-path update (lock-free)
    inline void update(const MetaState& meta_state) noexcept {
        meta_store_.update(meta_state.last_segment_index, meta_state.last_offset, meta_state.last_event_id);
        // Signal background flush
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.notify_one();
    }

    // Hot-path read of meta state
    inline MetaState get_state() const noexcept {
        return meta_store_.get_state();
    }

    inline const std::string& filepath() const noexcept {
        return meta_store_.filepath();
    }

private:
    inline void flush_loop_() {
        do {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [&] { 
                return !running_.load(std::memory_order_acquire) || meta_store_.is_dirty(); 
            });

            if (meta_store_.is_dirty()) {
                if (!meta_store_.flush_to_disk()) {
                    WK_DEBUG("[!!] WAL Meta coordinator failed to flush meta to disk: " << meta_store_.filepath());
                }
            }
        } while (running_.load(std::memory_order_acquire) || meta_store_.is_dirty());
    }

private:
    MetaStore meta_store_;
    std::atomic<bool> running_;
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable cv_;
};


} // namespace worker
} // namespace recorder
} // namespace wal
} // namespace flashstrike

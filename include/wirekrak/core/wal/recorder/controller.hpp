#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "lcr/log/logger.hpp"


namespace wirekrak {
namespace core {
namespace wal {
namespace recorder {

class Controller {
public:
    Controller() = default;
    ~Controller() { stop(); }

    // ---------------------------------------------------------------------
    // Public API
    // ---------------------------------------------------------------------

    /// Starts the background thread explicitly.
    /// If already running, this is a no-op.
    void start() {
        bool expected = false;
        if (running_.compare_exchange_strong(expected, true)) {
            worker_ = std::thread(&Controller::run_loop_, this);
            WK_INFO("[WAL] Recorder controller started.");
        }
    }

    /// Request stop and join the worker thread.
    void stop() {
        bool expected = true;
        if (running_.compare_exchange_strong(expected, false)) {
            {
                std::lock_guard<std::mutex> lk(mtx_);
                cv_.notify_one();
            }
        }
        if (worker_.joinable()) {
            worker_.join();
        }
        WK_INFO("[WAL] Recorder controller stopped.");
    }

    /// Notify the controller that a new recorder became active.
    /// The thread should wake immediately.
    void notify_work_available() {
        {
            std::lock_guard<std::mutex> lk(mtx_);
        }
        cv_.notify_one();
    }

    /// Increment the count of active recorders
    void increment_active() {
        active_recorders_.fetch_add(1, std::memory_order_relaxed);
        notify_work_available();
    }

    /// Decrement the count of active recorders
    void decrement_active() {
        active_recorders_.fetch_sub(1, std::memory_order_relaxed);
    }

    /// Set idle shutdown timeout (default: 5 minutes)
    void set_idle_shutdown(std::chrono::minutes timeout) noexcept {
        idle_shutdown_ = timeout;
    }

private:
    // ---------------------------------------------------------------------
    // Worker thread main loop
    // ---------------------------------------------------------------------
    void run_loop_() {
        using namespace std::chrono;
        auto last_active = steady_clock::now();

        while (running_) {
            uint32_t active = active_recorders_.load(std::memory_order_relaxed);

            if (active == 0) {
                // ---- IDLE MODE ----
                auto now = steady_clock::now();
                auto idle_time = now - last_active;
                if (idle_time >= idle_shutdown_) {
                    WK_INFO("[WAL] Idle shutdown triggered.");
                    break;
                }
                // If external stop → exit loop
                if (!running_) {
                    break;
                }
                // If we woke because work became active → update timestamp
                if (active_recorders_.load(std::memory_order_relaxed) > 0) {
                    last_active = std::chrono::steady_clock::now();
                    continue;
                }
                auto sleep_dur = adaptive_backoff_(idle_time);

                std::unique_lock<std::mutex> lk(mtx_);
                cv_.wait_for(lk, sleep_dur, [&] {
                    return !running_ ||
                           active_recorders_.load(std::memory_order_relaxed) > 0;
                });
                continue;
            }

            // ---- ACTIVE MODE ----
            last_active = steady_clock::now();

            // (NOT IMPLEMENTED YET)
            // Future:
            //   manager_.flush_pending_data();
            //
            // For now, sleep a bit to avoid burning CPU.
            std::this_thread::sleep_for(1ms);
        }
    }

    // ---------------------------------------------------------------------
    // Adaptive backoff policy
    // ---------------------------------------------------------------------
    static std::chrono::milliseconds adaptive_backoff_(
        std::chrono::steady_clock::duration idle_time)
    {
        using namespace std::chrono;

        if (idle_time < 100ms)   return 1ms;
        if (idle_time < 1s)      return 10ms;
        if (idle_time < 10s)     return 100ms;
        if (idle_time < 60s)     return 1000ms;
        return 5000ms; // long-term idle sleeping
    }

private:
    std::atomic<bool> running_{false};
    std::atomic<uint32_t> active_recorders_{0};

    std::chrono::minutes idle_shutdown_{std::chrono::minutes(5)};

    std::thread worker_;
    std::mutex mtx_;
    std::condition_variable cv_;
};

} // namespace recorder
} // namespace wal
} // namespace core
} // namespace wirekrak

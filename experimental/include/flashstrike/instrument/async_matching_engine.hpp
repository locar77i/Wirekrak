#pragma once

#include "flashstrike/globals.hpp"
#include "flashstrike/matching_engine/manager.hpp"
#include "flashstrike/wal/recorder/manager.hpp"
#include "flashstrike/instrument/telemetry/engine.hpp"
#include "lcr/metrics/snapshot/manager.hpp"
#include "lcr/system/cpu_relax.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/adaptive_backoff_until.hpp"
#include "lcr/log/Logger.hpp"

constexpr uint64_t ON_PROCESS_EVENT_PERIOD = 1ULL << 23;    // On every 8 million events (must be power of two)


namespace flashstrike {
namespace instrument {

// =====================================================================================
//  AsyncMatchingEngine — Ultra-Low-Latency orchestration layer for a single trading pair
// =====================================================================================
class AsyncMatchingEngine {
public:
    explicit AsyncMatchingEngine(uint64_t max_orders, const matching_engine::conf::Instrument& instrument)
        : instrument_(instrument)
        , snapshot_manager_(metrics_)
        , matching_engine_(max_orders, instrument, 256, metrics_.matching_engine) // 256 partitions
    {
    }

    ~AsyncMatchingEngine() {
        // Force clean shutdown if not already done
        stop_flag_.store(true, std::memory_order_release);
        // --- 1. Stop Matching Engine Thread ---
        if (matching_engine_thread_.joinable()) {
            WK_DEBUG("[WARN] Engine destructor: forcing matching-engine shutdown...");
            matching_engine_thread_.join();
        };
    }

    // Disable copy/move semantics
    AsyncMatchingEngine(const AsyncMatchingEngine&) = delete;
    AsyncMatchingEngine& operator=(const AsyncMatchingEngine&) = delete;
    AsyncMatchingEngine(AsyncMatchingEngine&&) = delete;
    AsyncMatchingEngine& operator=(AsyncMatchingEngine&&) = delete;


    [[nodiscard]] bool initialize() noexcept {
        stop_flag_.store(false, std::memory_order_release);
        matching_engine_thread_ = std::thread([this]{ matching_engine_thread_main_loop_(); });
        return true;
    }

    bool shutdown() noexcept {
        stop_flag_.store(true, std::memory_order_release);
        if (matching_engine_thread_.joinable()) {
            matching_engine_thread_.join();
        }
        return true;
    }

    inline bool submit_event(const RequestEvent& ev) noexcept {
        // Step 1. Validation (syntactic + semantic)
        if (!validate_(ev)) [[unlikely]] {
            return false;
        }
        // 2. Non-blocking enqueue for Matching Engine
        return lcr::adaptive_backoff_until(
            [&]() { return inbound_ring_.push(ev); },
            [&]() { return stop_flag_.load(std::memory_order_acquire); }
        );
    }

    // Accessors
    inline auto& trades_ring() noexcept { 
        return matching_engine_.trades_ring();
    }

    inline const auto& normalized_instrument() const noexcept {
        return matching_engine_.normalized_instrument();
    }

    [[nodiscard]] const telemetry::Engine& live_metrics() const noexcept {
        return metrics_;
    }

    [[nodiscard]] const telemetry::Engine& snapshot_metrics() const noexcept {
        return *snapshot_manager_.snapshot().data;
    }

    // Collect metrics for external exposition
    template <typename Collector>
    void collect(Collector& collector) const noexcept {
        snapshot_metrics().collect(instrument_.get_symbol('_'), collector);
    }

private:
    matching_engine::conf::Instrument instrument_;

    telemetry::Engine metrics_{};
    lcr::metrics::snapshot::Manager<telemetry::Engine> snapshot_manager_;

    matching_engine::Manager matching_engine_;

    lcr::lockfree::spsc_ring<RequestEvent, 1024> inbound_ring_{};
    std::thread matching_engine_thread_;

    std::atomic<bool> stop_flag_{false};

    // Helpers --------------------------------------------------------------------------

    [[nodiscard]] inline bool validate_(const RequestEvent& ev) const noexcept {
        (void)ev;
        return true;
    }

    // =============================================================================
    // process_event() — Ultra-Low-Latency (ULL) hot path for order event handling
    // =============================================================================
    inline bool process_event_(const RequestEvent& ev) noexcept {
        // Decode event and process in matching engine
        OrderIdx order_idx;
        OperationStatus status;
        switch (ev.type) {
            case RequestType::NEW_ORDER: {
                status = matching_engine_.process_order(ev.order_id, ev.order_type, ev.side, ev.price, ev.quantity, order_idx);
                break;
            }
            case RequestType::MODIFY_ORDER_PRICE: {
                status =  matching_engine_.modify_order_price(ev.order_id, ev.price);
                break;
            }
            case RequestType::MODIFY_ORDER_QUANTITY: {
                status =  matching_engine_.modify_order_quantity(ev.order_id, ev.quantity);
                break;
            }
            case RequestType::CANCEL_ORDER: {
                status =  matching_engine_.cancel_order(ev.order_id);
                break;
            }
            default:
                break;
        }
        on_process_event_();
        (void)status; // suppress unused variable warning
        return true;
    }

    void matching_engine_thread_main_loop_() noexcept {
        RequestEvent ev;
        size_t spins = 0;
        for (;;) {
            // 1. Try fast-path pop
            if (inbound_ring_.pop(ev)) [[likely]] {
                bool ok = process_event_(ev);
                if (!ok) {
                    WK_DEBUG("[ME Thread] Error processing event in matching engine.");
                }
                spins = 0;  // reset spin counter after successful pop
                continue;
            }
            // 2. Ring empty — check for shutdown
            if (stop_flag_.load(std::memory_order_acquire)) {
                // Flush remaining events if any
                while (inbound_ring_.pop(ev)) {
                     bool ok = process_event_(ev);
                    if (!ok) {
                        WK_DEBUG("[ME Thread] Error processing event in matching engine.");
                    }
                }
                break;
            }
            // 3. Idle wait strategy (adaptive spin + yield + sleep)
            if (spins < 2000) {
                lcr::system::cpu_relax(); // short pause
            } else if (spins < 10000) {
                std::this_thread::yield(); // short cooperative yield
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(50)); // back off a bit
            }
            ++spins;
        }

        WK_DEBUG("[ME Thread] Exiting cleanly.");
    }

    inline void on_process_event_() noexcept {
        static thread_local uint32_t counter = 0;
        counter++;
        if ((counter & (ON_PROCESS_EVENT_PERIOD - 1)) == 0) [[unlikely]] {
            matching_engine_.on_periodic_maintenance();
            snapshot_manager_.take_snapshot();
        }
    }
};

} // namespace instrument
} // namespace flashstrike

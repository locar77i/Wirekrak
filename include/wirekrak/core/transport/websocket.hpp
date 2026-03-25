#pragma once

#include <string>
#include <string_view>
#include <thread>
#include <functional>
#include <atomic>
#include <vector>
#include <cstring>
#include <cassert>
#include <immintrin.h>

#include "wirekrak/core/transport/error.hpp"
#include "wirekrak/core/transport/telemetry/websocket.hpp"
#include "wirekrak/core/transport/websocket/events.hpp"
#include "wirekrak/core/transport/websocket/backend_concept.hpp"
#include "wirekrak/core/policy/transport/websocket_bundle.hpp"
#include "wirekrak/core/config/transport/websocket.hpp"
#include "wirekrak/core/config/backpressure.hpp"
#include "wirekrak/core/telemetry.hpp"
#include "lcr/memory/footprint.hpp"
#include "lcr/buffer/concepts.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/system/thread_affinity.hpp"
#include "lcr/format.hpp"
#include "lcr/log/logger.hpp"
#include "lcr/trap.hpp"



namespace wirekrak::core::transport {

template<
    typename ControlRing,
    lcr::buffer::ProducerSpscRingConcept MessageRing,
    policy::transport::WebSocketBundleConcept PolicyBundle,
    websocket::BackendConcept Backend
>
class WebSocketImpl {
public:
    explicit WebSocketImpl(ControlRing& ctrl_ring, MessageRing& msg_ring, telemetry::WebSocket& telemetry) noexcept
        : control_ring_(ctrl_ring)
        , message_ring_(msg_ring)
        , telemetry_(telemetry) {
    }

    ~WebSocketImpl() {
        close();
    }

    // Connect to the specified WebSocket URL. Returns Error::None on successful connection, or an appropriate error code on failure.
    // connect() and close() must not be called concurrently
    [[nodiscard]]
    Error connect(std::string_view host, std::uint16_t port, std::string_view path, bool secure) noexcept {
        if (!backend_.connect(host, port, path, secure)) {
            WK_ERROR("[WS] connect failed");
            return Error::ConnectionFailed;
        }

        running_.store(true, std::memory_order_release);

        recv_thread_ = std::thread(&WebSocketImpl::receive_loop_, this);

        WK_TL1( telemetry_.connect_events_total.inc() );

        return Error::None;
    }

    // Send a text message. Returns true on success.
    // A boolean “accepted / not accepted” is the honest signal.
    // Errors are reported asynchronously via the error callback.
    [[nodiscard]]
    bool send(std::string_view msg) noexcept {
        if (!backend_.is_open()) [[unlikely]] {
            WK_ERROR("[WS] send() on closed WebSocket");
            return false;
        }

        WK_TRACE("[WS] Sending message ... (size: " << lcr::format_bytes_exact(msg.size()) << ")");

        if (!backend_.send(msg)) [[unlikely]] {
            WK_ERROR("[WS] send failed");
            WK_TL1( telemetry_.send_errors_total.inc() );
            return false;
        }

        WK_TL1( telemetry_.bytes_tx_total.inc(msg.size()) );
        WK_TL1( telemetry_.messages_tx_total.inc() );

        return true;
    }

    // Close (idempotent)
    // connect() and close() must not be called concurrently
    void close() noexcept {
        // Fast path: ensure only one thread executes shutdown
        if (closed_.exchange(true, std::memory_order_acq_rel)) [[unlikely]] {
            return;
        }

        // Stop receive loop
        running_.store(false, std::memory_order_release);

        // Close the backend (determinism depends on backend)
        backend_.close();

        // Join the receive thread to ensure all resources are cleaned up before close() returns
        if (recv_thread_.joinable()) {
            recv_thread_.join();
        }

        WK_TRACE("[WS] WebSocket closed.");
    }

    [[nodiscard]]
    bool backpressure_active() const noexcept {
        return backpressure_active_;
    }

    [[nodiscard]]
    telemetry::WebSocket& telemetry() noexcept {
        return telemetry_;
    }

    [[nodiscard]]
    lcr::memory::footprint memory_usage() const noexcept {
        return lcr::memory::footprint{
            .static_bytes = sizeof(*this),
            .dynamic_bytes = 0
        };
    }

    static void dump_configuration(std::ostream& os) noexcept {
        PolicyBundle::dump(os);
    }

private:
    // Telemetry reference (non-owning) 
    telemetry::WebSocket& telemetry_;

    // Backpressure policy instance
    using BackpressurePolicy = typename PolicyBundle::backpressure;

    // Hysteresis type based on the backpressure policy mode
    using Hysteresis = typename BackpressurePolicy::hysteresis;

    // Backpressure stabilizers (only used for non-zero-tolerance policies)
    [[no_unique_address]] Hysteresis ring_backpressure_;
    [[no_unique_address]] Hysteresis pool_backpressure_;

    // Global transport-level backpressure state (independent of the individual FSMs)
    bool backpressure_active_{false};

    // Compile-time backend
    Backend backend_;

    std::thread recv_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> closed_{false};
    std::atomic<bool> close_signaled_{false};

    // Control event queue (for signaling events like close, error, backpressure detected/cleared)
    ControlRing& control_ring_;

    // Data message queue (transport → connection/session)
    MessageRing& message_ring_;

    using slot_type = typename MessageRing::slot_type;
    using promotion_result_type = typename MessageRing::promotion_result_type;

private:

    // The receive loop is the heart of the transport's receive path.
    // Key features:
    // - Lock-free
    // - Zero-copy
    // - ULL-safe
    // - Deterministic
    void receive_loop_() noexcept {
        auto& clock = lcr::system::monotonic_clock::instance();

        lcr::system::pin_thread(0);

#ifdef WK_UNIT_TEST
    // Signal test that receive loop has started (for better synchronization in tests)
    if (receive_started_flag_) {
        receive_started_flag_->store(true, std::memory_order_release);
    }
#endif // WK_UNIT_TEST

        std::size_t message_count = 0;

        slot_type* current_slot = nullptr;

        while (running_.load(std::memory_order_acquire)) {
            //const bool samples_now = !(message_count & 0x3FF);
            const bool samples_now = true; // Always sample in tests to validate timing metrics

            // =========================================================
            // Acquire slot (start of new message)
            // =========================================================
            if (!current_slot) [[likely]] {
                current_slot = acquire_slot_();
                if (!current_slot) [[unlikely]] {
                    continue;  // backpressure handling (no slot available)
                }
                WK_TL3(
                    if (samples_now) [[unlikely]] {
                        current_slot->set_timestamp(clock.now_ns());
                    }
                );
                LCR_ASSERT_MSG(current_slot->size() == 0, "acquired slot must be empty");
            }
            // =========================================================
            // Promotion (buffer exhausted mid-message)
            // =========================================================
            else if (current_slot->remaining() == 0) [[unlikely]] {
                if (!promote_slot_(current_slot)) [[unlikely]] {
                    continue;
                }
            }

            // =========================================================
            // Read into slot (ZERO-COPY)
            // =========================================================

            WK_TL1( telemetry_.receive_calls_total.inc() );

            const auto result = backend_.read_some(current_slot->write_ptr(), current_slot->remaining());

            // =========================================================
            // CLOSE (graceful, NOT an error)
            // =========================================================
            if (result.frame == websocket::FrameType::Close) {
                WK_DEBUG("[WS] Received CLOSE frame");

                running_.store(false, std::memory_order_release);

                // Discard partial message
                if (current_slot) {
                    message_ring_.discard_producer_slot(current_slot);
                    current_slot = nullptr;
                }

                signal_close_();
                break;
            }

            // =========================================================
            // Contract validation
            // =========================================================
            LCR_ASSERT_MSG((result.status == websocket::ReceiveStatus::Ok) || (result.bytes == 0), "Backend violation: bytes must be 0 on non-Ok status");
            if (result.status == websocket::ReceiveStatus::Ok) [[likely]]{
                switch (result.frame) {
                    case websocket::FrameType::Fragment:
                        LCR_ASSERT_MSG(result.bytes > 0, "Backend violation: Fragment frame must have bytes > 0");
                        break;
                    case websocket::FrameType::Message:
                        // bytes >= 0 → always true, nothing to assert
                        break;
                    case websocket::FrameType::Close:
                        LCR_ASSERT_MSG(result.bytes == 0, "Backend violation: Close frame must have bytes == 0");
                        break;
                }
            }
            // =========================================================
            // ERRORS (failure-first)
            // =========================================================
            else {

                WK_TL1( telemetry_.rx_errors_total.inc() );

                Error error = Error::TransportFailure;

                switch (result.status) {

                    case websocket::ReceiveStatus::Timeout:
                        WK_WARN("[WS] Receive timeout");
                        error = Error::Timeout;
                        break;

                    case websocket::ReceiveStatus::ProtocolError:
                        WK_ERROR("[WS] Protocol error");
                        error = Error::ProtocolError;
                        break;

                    case websocket::ReceiveStatus::TransportError:
                    default:
                        WK_ERROR("[WS] Transport error");
                        error = Error::TransportFailure;
                        break;
                }

                // Emit error (failure-first model)
                if (!emit_event_(transport::websocket::Event::make_error(error))) {
                    WK_ERROR("[WS] Failed to emit error <" << to_string(error) << ">");
                }

                // Stop loop
                running_.store(false, std::memory_order_release);

                // Discard partial message if any
                if (current_slot) {
                    message_ring_.discard_producer_slot(current_slot);
                    current_slot = nullptr;
                }

                signal_close_();
                break;
            }

            WK_TL1( telemetry_.bytes_rx_total.inc(result.bytes) );

            if (result.bytes > 0) { // Commit bytes into slot
                current_slot->commit(result.bytes);
            }

            // =========================================================
            // Message boundary
            // =========================================================
            if (result.frame == websocket::FrameType::Message) {

                WK_TL1( telemetry_.messages_rx_total.inc() );
                WK_TL1( telemetry_.rx_message_bytes.set(current_slot->size()) );

                if (current_slot->is_external()) [[unlikely]] {
                    WK_TL1( telemetry_.external_buffers_total.inc() );
                }

                WK_TL3(
                    if (samples_now) [[unlikely]] {
                        auto start_ns = current_slot->timestamp();
                        auto now = clock.now_ns();
                        telemetry_.ws_message_assembly_duration.record(start_ns, now);
                        current_slot->set_timestamp(now);
                    }
                );

                message_ring_.commit_producer_slot();

                current_slot = nullptr;
                ++message_count;
            }
        }

        signal_close_();
    }

    [[nodiscard]]
    slot_type* acquire_slot_() noexcept {
        using core::policy::BackpressureMode;
        slot_type* slot; // it is always assigned because spins >= 1 (enforced by the policy concept)
        for (int i = 0; i < BackpressurePolicy::spins; ++i) {
            if ((slot = message_ring_.acquire_producer_slot())) break;
            _mm_pause();  // short burst -> absorbed by spin (~1-3µs)
        }
        if (!slot) [[unlikely]] { // persistent pressure -> enforce backpressure policy

            WK_TL1( telemetry_.message_ring_failures_total.inc() );

            // =========================================================
            // ZeroTolerance (transport-level forced close)
            // =========================================================
            if constexpr (BackpressurePolicy::mode == BackpressureMode::ZeroTolerance) {
                // 1.1. Forced close without signaling (transport decides fate)
                // ZeroTolerance guarantees no BackpressureDetected event will ever be emitted.
                WK_WARN("[WS] Backpressure detected (message ring saturated)");
                handle_fatal_error_("[WS] Failed to acquire message slot (zero-tolerance policy)"
                    " - transport correctness compromised (protocol is not draining fast enough)", Error::Backpressure);
                return nullptr;
            }
            // =========================================================
            // Strict / Relaxed (policy defines thresholds)
            // =========================================================
            else { 
                // 1.2. Immediate escalation (session decides fate)
                auto transition = ring_backpressure_.on_active_signal();
                if (transition == Hysteresis::Transition::Activated) {
                    WK_WARN("[WS] Backpressure detected (message ring saturated)");
                    update_backpressure_state_();
                }
                // Relax the CPU to give a chance to catch up the message ring before retrying
                // (scheduling another thread maybe more effective than a tight pause loop in this scenario)
                std::this_thread::yield();  // ~5-50µs
                return nullptr;
            }
        }
        // =============================================================
        // Successful slot acquisition
        // =============================================================
        if constexpr (BackpressurePolicy::mode != BackpressureMode::ZeroTolerance) {
            auto transition = ring_backpressure_.on_inactive_signal();
            if (transition == Hysteresis::Transition::Deactivated) {
                WK_TRACE("[WS] Backpressure cleared (message ring has available slots)");
                update_backpressure_state_();
            }
        }
        return slot;
    }

    [[nodiscard]]
    bool promote_slot_(slot_type*& slot) noexcept {
        using core::policy::BackpressureMode;

        WK_TL1( // Observability: track memory pool depth
            telemetry_.memory_pool_depth.set(message_ring_.memory_pool().used());
        );
    
        promotion_result_type result; // it is always assigned because spins >= 1 (enforced by the policy concept)
        for (int i = 0; i < BackpressurePolicy::spins; ++i) {
            if ((result = message_ring_.reserve(slot, config::transport::websocket::FRAME_SIZE_HINT)) <= promotion_result_type::Success) break;
            _mm_pause();  // short burst -> absorbed by spin (~1-3µs)
        }
        if (result > promotion_result_type::Success) [[unlikely]] {  // persistent pressure -> enforce backpressure policy

            WK_TL1( telemetry_.memory_pool_failures_total.inc() );
            
            // =========================================================
            // ZeroTolerance (transport-level forced close)
            // =========================================================
            if constexpr (BackpressurePolicy::mode == BackpressureMode::ZeroTolerance) {
                // 1.1. Forced close without signaling (transport decides fate)
                // ZeroTolerance guarantees no BackpressureDetected event will ever be emitted.
                WK_WARN("[WS] Backpressure detected (memory pool exhausted)");
                handle_fatal_error_("[WS] Failed to promote message slot (zero-tolerance policy)"
                    " - transport correctness compromised (protocol is not draining fast enough)", Error::Backpressure);
                message_ring_.discard_producer_slot(slot);
                slot = nullptr;
                return false;
            }
            // =========================================================
            // Strict / Relaxed / Custom (policy defines thresholds)
            // =========================================================
            else {
                // 1.2. Immediate escalation (session decides fate)
                auto transition = pool_backpressure_.on_active_signal();
                if (transition == Hysteresis::Transition::Activated) {
                    WK_WARN("[WS] Backpressure detected (memory pool exhausted)");
                    update_backpressure_state_();
                }
                // Relax the CPU to give a chance to catch up the memory pool before retrying
                // (scheduling another thread maybe more effective than a tight pause loop in this scenario)
                std::this_thread::yield();  // ~5-50µs
                return false;
            }
        }

        WK_TL1( telemetry_.slot_promotions_total.inc() );

        // =============================================================
        // Successful slot promotion
        // =============================================================
        if constexpr (BackpressurePolicy::mode != BackpressureMode::ZeroTolerance) {
            auto transition = pool_backpressure_.on_inactive_signal();
            if (transition == Hysteresis::Transition::Deactivated) {
                WK_TRACE("[WS] Backpressure cleared (memory pool has available slots)");
                update_backpressure_state_();
            }
        }
        return true;
    }

    bool emit_event_(transport::websocket::Event event) noexcept {
        WK_TL1( telemetry_.events_emitted_total.inc() );
        bool pushed = control_ring_.push(event);
        if (!pushed) [[unlikely]] {
            WK_TL1( telemetry_.control_ring_failures_total.inc() );
        }
        return pushed;
    }

    void emit_backpressure_detected_() noexcept {
        WK_TL1( telemetry_.backpressure_detected_total.inc() );
        if (!emit_event_(transport::websocket::Event::make_backpressure_detected())) {
            handle_fatal_error_("[WS] Failed to emit backpressure event (event lost)"
                " - transport correctness compromised (protocol is not draining fast enough)", Error::Backpressure);
        }
    }

    void emit_backpressure_cleared_() noexcept {
        WK_TL1( telemetry_.backpressure_cleared_total.inc() );
        if (!emit_event_(transport::websocket::Event::make_backpressure_cleared())) {
            // Event intentionally dropped on the floor (logged for observability)
            // We have already successfully acquired a slot, so transport correctness is not compromised
            WK_WARN("[WS] Dropping backpressure cleared event (control ring saturated)");
        }
    }

    void update_backpressure_state_() noexcept {
        const bool active = ring_backpressure_.is_active() || pool_backpressure_.is_active();
        // 1) Check if there is an actual state change (avoid emitting duplicate events)
        if (active == backpressure_active_) {
            return; // no state change
        }
        // 2) Update state and emit corresponding event
        backpressure_active_ = active;
        if (active) {
            emit_backpressure_detected_();
        } else {
            emit_backpressure_cleared_();
        }
    }

    void signal_close_() noexcept {
        // Ensure close callback is invoked exactly once
        if (close_signaled_.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        WK_TL1( telemetry_.close_events_total.inc() );
        if (!emit_event_(transport::websocket::Event::make_close())) {
            WK_ERROR("[WS] Failed to emit close event (lost in transport shutdown)");
        }
    }

    void handle_fatal_error_(const char* message, Error error) noexcept {
        WK_WARN(message);
        // 1. Ensure only one thread performs fatal shutdown
        // Future backpresusre policy (default:strict
        // Wirekrak should never lie to the user or perform magic without explicit user instruction
        // Defensive action: close the connection to prevent further damage
        if (!running_.exchange(false, std::memory_order_acq_rel)) {
            return; // already shutting down
        }
        WK_FATAL("[WS] Forcing transport close to preserve correctness guarantees.");
        // 2. Emit error event if possible
        if (!emit_event_(transport::websocket::Event::make_error(error))) {
            WK_ERROR("[WS] Failed to emit error <" << to_string(error) << ">  - Event lost in transport shutdown");
        }
        // 3. Signal close to ensure transport is fully closed (exactly-once guarded)
        signal_close_();
    }

#ifdef WK_UNIT_TEST
public:
    // Test-only accessor to the internal backend
    Backend& test_backend() noexcept {
        return backend_;
    }

    [[nodiscard]]
    bool poll_event(transport::websocket::Event& out) noexcept {
        return control_ring_.pop(out);
    }

    [[nodiscard]]
    slot_type* peek_message() noexcept {
        return message_ring_.peek_consumer_slot();
    }

    void release_message(slot_type* slot) noexcept {
        message_ring_.release_consumer_slot(slot);
    }

public:
    // Test-only method to start receive loop without connect()
    void test_start_receive_loop() noexcept {
        WK_TRACE("[WS:TEST] Connecting WebSocket (simulated) ...");
        LCR_ASSERT_MSG(!test_receive_loop_started_, "test_start_receive_loop() called twice");
        test_receive_loop_started_ = true;
        // Fake non-null WebSocket handle
        running_.store(true, std::memory_order_release);
        recv_thread_ = std::thread(&WebSocketImpl::receive_loop_, this);
    }

private:
    bool test_receive_loop_started_ = false;

public:
    // Test-only hook: signals when receive_loop_() starts
    //
    // Debug builds exposed a race in the test harness.
    // Fixed it by adding a test-only synchronization hook to the transport so
    // tests wait on real transport state instead of timing assumptions.
    void set_receive_started_flag(std::atomic<bool>* flag) noexcept {
        receive_started_flag_ = flag;
    }

private:
    std::atomic<bool>* receive_started_flag_ = nullptr;
#endif // WK_UNIT_TEST

};

} // namespace wirekrak::core::transport



// ============================================================================================
// Public alias with backend selection
// ============================================================================================



// -----------------------------------------------------------------------------
// Backend selection
// -----------------------------------------------------------------------------

#if defined(WIREKRAK_FORCE_ASIO)
    #define WK_BACKEND_ASIO
#elif defined(WIREKRAK_FORCE_WINHTTP)
    #define WK_BACKEND_WINHTTP
#else
    #if defined(_WIN32)
        #define WK_BACKEND_WINHTTP
    #elif defined(__linux__)
        #define WK_BACKEND_ASIO
    #else
        #error "Unsupported platform"
    #endif
#endif

// -----------------------------------------------------------------------------
// Includes
// -----------------------------------------------------------------------------

#if defined(WK_BACKEND_WINHTTP)
    #include "wirekrak/core/transport/winhttp/backend.hpp"
    //#include "wirekrak/core/transport/asio/backend.hpp"
#elif defined(WK_BACKEND_ASIO)
    #include "wirekrak/core/transport/asio/backend.hpp"
#endif

namespace wirekrak::core::transport {

// -----------------------------------------------------------------------------
// Public WebSocket alias
// -----------------------------------------------------------------------------

#if defined(WK_BACKEND_WINHTTP)
    using DefaultBackend = winhttp::Backend;
    //using DefaultBackend = asio::Backend;
#elif defined(WK_BACKEND_ASIO)
    using DefaultBackend = asio::Backend;
#endif


template<
        typename ControlRing,
        typename MessageRing,
        typename PolicyBundle = policy::transport::DefaultWebsocket,
        typename Backend = DefaultBackend
    >
    using WebSocket = WebSocketImpl<
        ControlRing,
        MessageRing,
        PolicyBundle,
        Backend
    >;

} // namespace wirekrak::core::transport

#pragma once

/*
================================================================================
WebSocket Transport (WinHTTP minimal implementation)
================================================================================

This header implements the Wirekrak WebSocket transport using WinHTTP, following
a strict separation between *transport mechanics* and *connection policy*.

Design highlights:
  • Single-connection transport primitive — no retries, no reconnection logic
  • Policy-free by design — recovery and subscription replay live in the Client
  • Failure-first signaling — transport errors and close frames are propagated
    immediately and exactly once
  • Deterministic lifecycle — idempotent close() and explicit state transitions
  • Testability by construction — WinHTTP calls are injected as a compile-time
    policy (WebSocketImpl<ApiConcept>), enabling unit tests without OS or network

The templated design allows the same WebSocket implementation to be exercised
against a fake WinHTTP backend in unit tests, while remaining zero-overhead and
fully inlined in production builds.

This approach mirrors production-grade trading SDKs, where transport correctness
is validated independently from the operating system and network stack.
================================================================================
*/

#include <string>
#include <string_view>
#include <thread>
#include <functional>
#include <atomic>
#include <vector>
#include <cassert>
#include <immintrin.h>

#include "wirekrak/core/transport/error.hpp"
#include "wirekrak/core/transport/winhttp/real_api.hpp"
#include "wirekrak/core/transport/telemetry/websocket.hpp"
#include "wirekrak/core/transport/websocket/events.hpp"
#include "wirekrak/core/policy/transport/websocket_bundle.hpp"
#include "wirekrak/core/config/transport/websocket.hpp"
#include "wirekrak/core/config/backpressure.hpp"
#include "wirekrak/core/telemetry.hpp"
#include "lcr/memory/footprint.hpp"
#include "lcr/buffer/concepts.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/format.hpp"
#include "lcr/log/logger.hpp"
#include "lcr/trap.hpp"

#include <windows.h>
#include <winhttp.h>
#include <winerror.h>


// helper to convert UTF-8 string to wide string
inline std::wstring to_wide(std::string_view utf8) {
    if (utf8.empty()) [[unlikely]] {
        return {};
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (size <= 0) [[unlikely]] {
        return {};
    }
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), size);
    return out;
}


namespace wirekrak::core::transport::winhttp {

template<
    typename ControlRing,
    lcr::buffer::ProducerSpscRingConcept MessageRing,
    policy::transport::WebSocketBundleConcept PolicyBundle = policy::transport::DefaultWebsocket,
    ApiConcept Api = RealApi
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
        if (hSession_) {
            WinHttpCloseHandle(hSession_);
            hSession_ = nullptr;
        }
    }

    [[nodiscard]]
    Error connect(std::string_view host, std::uint16_t port, std::string_view path, bool secure) noexcept {
        hSession_ = WinHttpOpen(
            L"Wirekrak/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );
        if (!hSession_) {
            WK_ERROR("[WS] WinHttpOpen failed");
            return Error::TransportFailure;
        }

        host_w_ = to_wide(host);
        path_w_ = to_wide(path);

        hConnect_ = WinHttpConnect(hSession_, host_w_.c_str(), port, 0);
        if (!hConnect_) {
            WK_ERROR("[WS] WinHttpConnect failed");
            return Error::ConnectionFailed;
        }

        DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;

        hRequest_ = WinHttpOpenRequest(
            hConnect_,
            L"GET",
            path_w_.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            flags
        );

        if (!hRequest_) {
            WK_ERROR("[WS] WinHttpOpenRequest failed");
            return Error::TransportFailure;
        }

        if (!WinHttpSetOption(hRequest_, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)) {
            WK_ERROR("[WS] WinHttpSetOption failed");
            return Error::ProtocolError;
        }

        if (!WinHttpSendRequest(hRequest_, nullptr, 0, nullptr, 0, 0, 0)) {
            WK_ERROR("[WS] WinHttpSendRequest failed");
            return Error::HandshakeFailed;
        }

        if (!WinHttpReceiveResponse(hRequest_, nullptr)) {
            WK_ERROR("[WS] WinHttpReceiveResponse failed");
            return Error::HandshakeFailed;
        }

        hWebSocket_ = WinHttpWebSocketCompleteUpgrade(hRequest_, 0);
        if (!hWebSocket_) {
            WK_ERROR("[WS] WinHttpWebSocketCompleteUpgrade failed");
            return Error::HandshakeFailed;
        }

        running_.store(true, std::memory_order_relaxed);
        closed_.store(false, std::memory_order_relaxed);
        recv_thread_ = std::thread(&WebSocketImpl::receive_loop_, this);

        WK_TL1( telemetry_.connect_events_total.inc() );

        return Error::None;
    }

    // Send a text message. Returns true on success.
    // A boolean “accepted / not accepted” is the honest signal.
    // Errors are reported asynchronously via the error callback.
    [[nodiscard]]
    bool send(std::string_view msg) noexcept {
        // 1. Check preconditions (connected state)
        if (!hWebSocket_) [[unlikely]] {
            WK_ERROR("[WS] send() called on unconnected WebSocket");
            return false;
        }
        // 2. Call WebSocket send API
        WK_TRACE("[WS:API] Sending message ... (size: " << lcr::format_bytes_exact(msg.size()) << ")");
        const DWORD result = api_.websocket_send(
            hWebSocket_,
            WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
            const_cast<char*>(msg.data()),
            static_cast<DWORD>(msg.size())
        );
        // 3. Handle possible errors
        if (result != ERROR_SUCCESS) [[unlikely]] {
            WK_ERROR("[WS] websocket_send() failed");
            WK_TL1( telemetry_.send_errors_total.inc() );
            return false;
        }
        // 4. Update telemetry
        WK_TL1( telemetry_.bytes_tx_total.inc(msg.size()) );
        WK_TL1( telemetry_.messages_tx_total.inc() );
        return true;
    }

    // Close - No guards and no early return (idempotent)
    void close() noexcept {
        // 1. Stop receive loop
        running_.store(false, std::memory_order_release);

        // 2. Cancel blocking receive (if still active)
        if (hWebSocket_) {
            WK_TRACE("[WS:API] Closing WebSocket ...");
            api_.websocket_close(hWebSocket_);
        }

        // 3. Join thread if joinable
        if (recv_thread_.joinable()) {
            recv_thread_.join();
        }

        // 4. Always release handles deterministically
        if (hWebSocket_) { WinHttpCloseHandle(hWebSocket_); hWebSocket_ = nullptr; }
        if (hRequest_)   { WinHttpCloseHandle(hRequest_);   hRequest_ = nullptr; }
        if (hConnect_)   { WinHttpCloseHandle(hConnect_);   hConnect_ = nullptr; }
        //if (hSession_)   { WinHttpCloseHandle(hSession_);   hSession_ = nullptr; }
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

    // Compile-time API policy (default: RealApi)
    Api api_;

    std::wstring host_w_;
    std::wstring path_w_;

    // WinHTTP handles
    HINTERNET hSession_   = nullptr;
    HINTERNET hConnect_   = nullptr;
    HINTERNET hRequest_   = nullptr;
    HINTERNET hWebSocket_ = nullptr;

    std::thread recv_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> closed_{false};

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
#ifdef WK_UNIT_TEST
        // Debug builds exposed a race in the test harness.
        // Fixed it by adding a test-only synchronization hook to the transport so
        // tests wait on real transport state instead of timing assumptions.
        if (receive_started_flag_) {
            receive_started_flag_->store(true, std::memory_order_release);
        }
#endif // WK_UNIT_TEST

        uint32_t fragments = 0;
        slot_type* current_slot = nullptr;
        
        // Receive internal loop
        while (running_.load(std::memory_order_acquire)) {
            DWORD bytes = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE type;

            // === Lazy slot acquisition (start of new message) ===
            if (!current_slot) [[likely]] {
                current_slot = acquire_slot_(); 
                if (!current_slot) [[unlikely]] {
                    continue; // backpressure handling (no slot available)
                }
                LCR_ASSERT_MSG(current_slot->size() == 0, "On receive loop - acquired slot should be empty");
            }
            // === Reactive promotion (ONLY if not starting a new message and capacity is exhausted) ===
            else if (current_slot->remaining() == 0) [[unlikely]] {
                if (!promote_slot_(current_slot)) {
                    continue; // backpressure handling (failed to promote)
                }
            }

            // === Amount writable this iteration ===
            DWORD writable = static_cast<DWORD>(current_slot->remaining());
            if (writable == 0) [[unlikely]] {
                handle_fatal_error_("[WS] Failed to receive incoming message (size exceeds buffer capacity)"
                    " - transport correctness compromised (message will be truncated)", Error::ProtocolError);
                // abandon current slot (not committed)
                message_ring_.discard_producer_slot(current_slot);
                current_slot = nullptr;
                break;
            }

            // === Call WebSocket receive API ===
            //WK_TRACE("[WS:API] Blocking on WebSocket receive ...");
            WK_TL1( telemetry_.receive_calls_total.inc() );
            DWORD result = api_.websocket_receive(
                hWebSocket_,
                current_slot->write_ptr(),
                writable,
                &bytes,
                &type
            );

            // === Handle errors ===
            if (result != ERROR_SUCCESS) [[unlikely]] { // abnormal termination
                WK_TL1( telemetry_.rx_errors_total.inc() );
                auto error = handle_receive_error_(result);
                if (!emit_event_(websocket::Event::make_error(error))) {
                    WK_ERROR("[WS] Failed to emit error <" << to_string(error) << ">  - Event lost in transport shutdown");
                }
                // Stop the loop and signal close
                running_.store(false, std::memory_order_release);
                signal_close_();
                // abandon current slot (not committed)
                message_ring_.discard_producer_slot(current_slot);
                current_slot = nullptr;
                break;
            }

            // === Successful receive ===
            // bytes_rx_total counts raw bytes received from the WebSocket API, including fragments and control frames.
            WK_TL1( telemetry_.bytes_rx_total.inc(bytes) );

            // === Handle close frame ===
            if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) { // normal termination
                WK_DEBUG("[WS] Received WebSocket close frame.");
                // Stop the loop and signal close
                running_.store(false, std::memory_order_release);
                signal_close_();
                // abandon current slot (not committed)
                message_ring_.discard_producer_slot(current_slot);
                current_slot = nullptr;
                break;
            }

            // === Commit received bytes into slot ===
            auto total_bytes = current_slot->commit(bytes);

            // === Handle final message frame ===
            if (type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE || type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) [[likely]] {
                if (fragments > 0) {
                    WK_TRACE("[WS] Received final message fragment (size: " << lcr::format_bytes_exact(bytes) << ", total message size: "
                        << lcr::format_bytes_exact(total_bytes) << ", fragments: " << fragments << ")");
                    WK_TL1( telemetry_.rx_fragments_total.inc() );
                }
                WK_TL1( telemetry_.rx_message_bytes.set(total_bytes) );
                WK_TL1( telemetry_.messages_rx_total.inc() );
                WK_TL1( telemetry_.fragments_per_message.record(fragments + 1) );
                // Commit the complete message to the ring buffer (and reset state for the next message)
                if (current_slot->is_external()) [[unlikely]] {
                    WK_TL1( telemetry_.external_buffers_total.inc() );
                    WK_TRACE("[WS] Received message exceeded internal buffer capacity and was written directly into an external buffer (size: "
                        << lcr::format_bytes_exact(current_slot->size()) << ")");
                }

                message_ring_.commit_producer_slot();
                current_slot = nullptr;
                fragments = 0;
            }
            else // === Handle message fragments ===
            if (type == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE || type == WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE) {
                WK_TRACE("[WS] Received message fragment (size: " << lcr::format_bytes_exact(bytes) << ")");
                WK_TL1( telemetry_.rx_fragments_total.inc() );
                ++fragments;
            }
        }
        // If we exit loop without explicit close signaling
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
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

    bool emit_event_(websocket::Event event) noexcept {
        WK_TL1( telemetry_.events_emitted_total.inc() );
        bool pushed = control_ring_.push(event);
        if (!pushed) [[unlikely]] {
            WK_TL1( telemetry_.control_ring_failures_total.inc() );
        }
        return pushed;
    }

    void emit_backpressure_detected_() noexcept {
        WK_TL1( telemetry_.backpressure_detected_total.inc() );
        if (!emit_event_(websocket::Event::make_backpressure_detected())) {
            handle_fatal_error_("[WS] Failed to emit backpressure event (event lost)"
                " - transport correctness compromised (protocol is not draining fast enough)", Error::Backpressure);
        }
    }

    void emit_backpressure_cleared_() noexcept {
        WK_TL1( telemetry_.backpressure_cleared_total.inc() );
        if (!emit_event_(websocket::Event::make_backpressure_cleared())) {
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

    [[nodiscard]]
    Error handle_receive_error_(DWORD error) noexcept {
        switch (error) {
        case ERROR_WINHTTP_OPERATION_CANCELLED: // ERR_OPERATION_CANCELLED 12017 (local close)
            // Local shutdown, expected during close()
            WK_TRACE("[WS] Receive cancelled (local shutdown)");
            return Error::LocalShutdown;

        case ERROR_WINHTTP_CONNECTION_ERROR: // ERR_CONNECTION_ABORTED 12030 (peer closed)
            // Remote closed connection (no CLOSE frame)
            WK_DEBUG("[WS] Connection closed by peer");
            return Error::RemoteClosed;

        case ERROR_WINHTTP_TIMEOUT: // ERR_TIMED_OUT 12002 (timeout)
            // Network stalled or idle timeout
            WK_WARN("[WS] Receive timeout");
            return Error::Timeout;

        case ERROR_WINHTTP_CANNOT_CONNECT: // ERR_CONNECTION_FAILED 12029 (connect failed)
            // Usually handshake or DNS issues
            WK_ERROR("[WS] Cannot connect to remote host");
            return Error::ConnectionFailed;

        case ERROR_WINHTTP_INVALID_SERVER_RESPONSE: // ERR_INVALID_RESPONSE 12152 (invalid response)
            // Protocol error or unexpected server response
            WK_ERROR("[WS] Invalid response from server");
            return Error::ProtocolError;

        default:
            // Anything else is unexpected
            WK_ERROR("[WS] Receive failed with error code " << error);
            return Error::TransportFailure;
        }
    }

    void signal_close_() noexcept {
        // Ensure close callback is invoked exactly once
        if (closed_.exchange(true)) {
            return;
        }
        WK_TL1( telemetry_.close_events_total.inc() );
        if (!emit_event_(websocket::Event::make_close())) {
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
        if (!emit_event_(websocket::Event::make_error(error))) {
            WK_ERROR("[WS] Failed to emit error <" << to_string(error) << ">  - Event lost in transport shutdown");
        }
        // 3. Signal close to ensure transport is fully closed (exactly-once guarded)
        signal_close_();
    }

#ifdef WK_UNIT_TEST
public:
    // Test-only accessor to the internal API
    Api& test_api() noexcept {
        return api_;
    }

    [[nodiscard]]
    bool poll_event(websocket::Event& out) noexcept {
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
        hWebSocket_ = reinterpret_cast<HINTERNET>(1);
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

} // namespace wirekrak::core::transport::winhttp

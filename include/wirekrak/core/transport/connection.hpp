#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <atomic>
#include <chrono>
#include <memory>

#include "wirekrak/core/transport/concepts.hpp"
#include "wirekrak/core/transport/telemetry/connection.hpp"
#include "wirekrak/core/transport/parse_url.hpp"
#include "wirekrak/core/transport/state.hpp"
#include "wirekrak/core/telemetry.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak::core {
namespace transport {

/*
===============================================================================
 wirekrak::core::transport::Connection
===============================================================================

Generic transport-level connection abstraction, parameterized by a WebSocket
transport implementation conforming to transport::WebSocketConcept.

A Connection represents a *logical* connection whose identity remains stable
across transient transport failures and automatic reconnections.

This component encapsulates all *connection-level* concerns and is designed
to be reused across protocols (Kraken, future exchanges, custom feeds).

It is intentionally decoupled from any exchange schema or message format.

-------------------------------------------------------------------------------
 Responsibilities
-------------------------------------------------------------------------------
- Establish and manage a logical connection over a WebSocket transport
- Dispatch raw text frames to higher-level protocol clients
- Detect connection liveness using heartbeat and message activity
- Automatically reconnect with exponential backoff on failures
- Provide deterministic, poll-driven behavior (no threads, no timers)

-------------------------------------------------------------------------------
 Design Guarantees
-------------------------------------------------------------------------------
- No inheritance and no virtual functions
- Zero runtime polymorphism (concept-based design)
- Header-only, zero-cost abstractions
- Transport-agnostic via transport::WebSocketConcept
- Fully testable using mock transports
- No background threads; all logic is driven via poll()

-------------------------------------------------------------------------------
 Liveness & Reconnection Model
-------------------------------------------------------------------------------
- Two independent signals are tracked:
    * Last message timestamp
    * Last heartbeat timestamp
- A reconnect is triggered only if BOTH signals are stale
- Transport is force-closed to reuse the same reconnection state machine
- Reconnection uses bounded exponential backoff
- Subscriptions are replayed by higher-level protocol sessions

-------------------------------------------------------------------------------
 Usage Model
-------------------------------------------------------------------------------
- Call open(url) once to activate the connection
- Register callbacks (on_message, on_disconnect, on_liveness_timeout)
- Drive progress by calling poll() regularly
- Compose this Connection inside protocol-specific sessions (e.g. Kraken)

-------------------------------------------------------------------------------
 Notes
-------------------------------------------------------------------------------
- URL parsing is intentionally minimal (ws:// and wss:// only)
- TLS is delegated to the underlying transport (e.g. WinHTTP + SChannel)
- This class is safe to unit-test without any real network access

===============================================================================
*/

// Context structure for retry callbacks
// Provides information about the retry attempt.
struct RetryContext {
    std::string_view url;
    Error error;
    int attempt;
    std::chrono::milliseconds next_delay;
};


template <transport::WebSocketConcept WS>
class Connection {
    static constexpr auto HEARTBEAT_TIMEOUT = std::chrono::seconds(15); // default heartbeat timeout
    static constexpr auto MESSAGE_TIMEOUT   = std::chrono::seconds(15); // default message timeout
    static constexpr auto LIVENESS_WARNING_RATIO = 0.8;                 // warn when 80% of liveness window is elapsed

public:
    using message_handler_t          = std::function<void(std::string_view)>;
    using connect_handler_t          = std::function<void()>;
    using disconnect_handler_t       = std::function<void()>;
    using liveness_handler_t         = std::function<void()>;
    using liveness_warning_handler_t = std::function<void(std::chrono::milliseconds remaining)>;
    using retry_handler_t            = std::function<void(const RetryContext&)>;

public:
    Connection(telemetry::Connection& telemetry,
               std::chrono::seconds heartbeat_timeout = HEARTBEAT_TIMEOUT,
               std::chrono::seconds message_timeout = MESSAGE_TIMEOUT,
               double liveness_warning_ratio = LIVENESS_WARNING_RATIO) noexcept
        : telemetry_(telemetry)
        , heartbeat_timeout_(heartbeat_timeout)
        , message_timeout_(message_timeout)
        , liveness_warning_ratio_(liveness_warning_ratio)
    {
        recompute_liveness_windows_();
    }

    // Ensure transport is closed on destruction.
    // Reconnection is not attempted after object lifetime ends.
    ~Connection() {
        close();
    }

    // Connection lifecycle
    [[nodiscard]]
    inline Error open(const std::string& url) noexcept {
        WK_TL1( telemetry_.open_calls_total.inc() ); // This represents explicit caller intent
        if (get_state_() != State::Disconnected && get_state_() != State::WaitingReconnect) {
            WK_WARN("[CONN] open() called while not disconnected  (state: " << to_string(get_state_()) << "). Ignoring.");
            return Error::InvalidState;
        }
        last_url_ = url;
        // 1) Enter connecting state
        set_state_(State::Connecting);
        // 2) Parse and validate URL
        ParsedUrl parsed_url;
        auto error = parse_url(url, parsed_url);
        if (error != Error::None) {
            WK_ERROR("[CONN] URL parsing failed");
            last_error_.store(error, std::memory_order_relaxed);
            set_state_(State::Disconnected);
            return error;
        }
        // 3) Create a fresh transport instance
        create_transport_();
        // 4) Attempt reconnection
        WK_INFO("[CONN] Connecting to: " << url);
        error = ws_->connect(parsed_url.host, parsed_url.port, parsed_url.path);
        last_error_.store(error, std::memory_order_relaxed);
        if (error != Error::None) {
            WK_TL1( telemetry_.connect_failure_total.inc() );
            if (should_retry_(error)) {
                WK_TL1( telemetry_.retry_cycles_started_total.inc() ); // This counts retry cycles, not attempts
                set_state_(State::WaitingReconnect);
                // Retry immediately
                arm_immediate_reconnect_(error);
            }
            else {
                set_state_(State::Disconnected);
            }
            WK_ERROR("[CONN] Connection failed (" << to_string(error) << ")");
            return error;
        }
        // 5) Enter connected state
        WK_TL1( telemetry_.connect_success_total.inc() ); // This reflects a state-machine fact, not a transport fact.
        WK_INFO("[CONN] Connected successfully.");
        enter_connected_state_();
        return Error::None;
    }

    // Manual disconnec - tclose() performs an unconditional shutdown and
    // cancels any pending reconnection attempts.
    inline void close() noexcept {
        WK_TL1( telemetry_.close_calls_total.inc() ); // This represents explicit user intent
        // Guard close() by state
        const auto state = get_state_();
        if (state == State::Disconnected) {
            return; // idempotent
        }
        if (state == State::Disconnecting) {
            return; // already closing
        }
        // Proceed to close
        set_state_(State::Disconnecting);
        if (ws_) {
            // close() does not cancel pending retry state explicitly;
            // retry resolution is handled by transport close callback.
            ws_->close();
        }
        else {
            set_state_(State::Disconnected);
        }
    }

    // Sending
    [[nodiscard]]
    inline bool send(std::string_view text) noexcept {
        WK_TL1( telemetry_.send_calls_total.inc() ); // This represents explicit user intent
        if (get_state_() != State::Connected) {
            WK_WARN("[CONN] send() called while not connected (state: " << to_string(get_state_()) << "). Ignoring.");
            WK_TL1( telemetry_.send_rejected_total.inc() ); // Reflects connection-level gating
            return false;
        }
        if (ws_->send(std::string(text))) {
            last_message_ts_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    // // Event loop
    inline void poll() noexcept {
        if (get_state_() == State::Connected) {
            // === Liveness warning check ===
            if (!liveness_warning_fired_ && hooks_.on_liveness_warning_cb_) {
                auto remaining = liveness_remaining_();
                // Check if we are within the liveness danger window
                if (remaining <= liveness_danger_window_ && remaining.count() > 0) {
                    WK_TRACE("[CONN] Liveness warning: " << remaining.count() << "ms remaining.");
                    liveness_warning_fired_ = true;
                    hooks_.on_liveness_warning_cb_(remaining);
                }
            }
            // === Liveness timeout check ===
            if (is_liveness_stale_()) {
                WK_TL1( telemetry_.liveness_timeouts_total.inc() ); // the decision is made
                last_error_.store(Error::Timeout, std::memory_order_relaxed);
                WK_TRACE("[CONN] Liveness timeout: No protocol traffic observed within liveness window (Forcing reconnect).");
                set_state_(State::ForcedDisconnection);
                if (hooks_.on_liveness_timeout_cb_) {
                    hooks_.on_liveness_timeout_cb_();
                }
                // Force transport failure → triggers reconnection
                ws_->close();
            }
        }
        // === Reconnection logic ===
        auto now = std::chrono::steady_clock::now();
        if (get_state_() == State::WaitingReconnect && now >= next_retry_) {
            if (!reconnect_()) {
                const auto error = last_error_.load(std::memory_order_relaxed);
                if (should_retry_(error)) {
                    // Schedule next retry with backoff
                    schedule_next_retry_();
                } else {
                    set_state_(State::Disconnected);
                }
            }
        }
    }

    // Callbacks
    void on_message(message_handler_t cb) noexcept {
        hooks_.on_message_cb_ = std::move(cb);
    }

    void on_connect(connect_handler_t cb) noexcept {
        hooks_.on_connect_cb_ = std::move(cb);
    }

    void on_disconnect(disconnect_handler_t cb) noexcept {
        hooks_.on_disconnect_cb_ = std::move(cb);
    }

    void on_liveness_warning(liveness_warning_handler_t cb) noexcept {
        hooks_.on_liveness_warning_cb_ = std::move(cb);
    }
    
    void on_liveness_timeout(liveness_handler_t cb) noexcept {
        hooks_.on_liveness_timeout_cb_ = std::move(cb);
    }

    void on_retry(retry_handler_t cb) noexcept {
        hooks_.on_retry_cb_ = std::move(cb);
    }

    // Accessors
    [[nodiscard]]
    inline State get_state() const noexcept {
        return state_;
    }

    [[nodiscard]]
    inline std::atomic<uint64_t>& heartbeat_total() noexcept {
        return heartbeat_total_;
    }

    [[nodiscard]]
    inline const std::atomic<uint64_t>& heartbeat_total() const noexcept {
        return heartbeat_total_;
    }

    [[nodiscard]]
    inline std::atomic<std::chrono::steady_clock::time_point>& last_heartbeat_ts() noexcept {
        return last_heartbeat_ts_;
    }

    [[nodiscard]]
    inline const std::atomic<std::chrono::steady_clock::time_point>& last_heartbeat_ts() const noexcept {
        return last_heartbeat_ts_;
    }

    // Mutators
    inline void set_liveness_timeout(std::chrono::milliseconds heartbeat, std::chrono::milliseconds message) noexcept {
        heartbeat_timeout_ = heartbeat;
        message_timeout_   = message;
        recompute_liveness_windows_();
    }

#ifdef WK_UNIT_TEST
public:
    inline void force_last_message(std::chrono::steady_clock::time_point ts) noexcept{
        last_message_ts_.store(ts, std::memory_order_relaxed);
    }

    inline void force_last_heartbeat(std::chrono::steady_clock::time_point ts) noexcept {
        last_heartbeat_ts_.store(ts, std::memory_order_relaxed);
    }

    inline const std::atomic<std::chrono::steady_clock::time_point>& get_last_message_ts() const noexcept {
        return last_message_ts_;
    }

    inline const std::atomic<std::chrono::steady_clock::time_point>& get_last_heartbeat_ts() const noexcept {
        return last_heartbeat_ts_;
    }

    WS& ws() {
        return *ws_;
    }
#endif // WK_UNIT_TEST

private:
    std::string last_url_;
    transport::telemetry::Connection& telemetry_;
    std::unique_ptr<WS> ws_;

    // Heartbeat messages are used as a deterministic liveness signal that drives reconnection.
    // If no heartbeat is received for N seconds:
    // - Assume the connection is unhealthy (even if TCP is still “up”)
    // - Force-close the WebSocket
    // - Let the existing reconnection state machine recover
    // - Replay subscriptions automatically
    //
    // Benefits:
    // - Simple liveness detection
    // - Decouples transport health from protocol health
    // - No threads. No timers. Poll-driven.
    std::atomic<uint64_t> heartbeat_total_;
    std::atomic<std::chrono::steady_clock::time_point> last_heartbeat_ts_;
    // TODO: remove atomicity if not needed
    std::atomic<std::chrono::steady_clock::time_point> last_message_ts_;

    std::chrono::milliseconds heartbeat_timeout_{10000};
    std::chrono::milliseconds message_timeout_{15000};
    double liveness_warning_ratio_;
    std::chrono::milliseconds liveness_danger_window_;

    // Hooks structure to store all user-defined callbacks
    struct Hooks {
        message_handler_t    on_message_cb_{};
        connect_handler_t    on_connect_cb_{};
        disconnect_handler_t on_disconnect_cb_{};
        liveness_warning_handler_t on_liveness_warning_cb_{};
        liveness_handler_t   on_liveness_timeout_cb_{};
        retry_handler_t      on_retry_cb_{};
    };

    // Handlers bundle
    Hooks hooks_;

    std::atomic<Error> last_error_{Error::None};
    std::atomic<Error> retry_root_error_{Error::None};

    bool liveness_warning_fired_{false};

    // State machine 
    State state_{State::Disconnected};
    std::chrono::steady_clock::time_point next_retry_{};
    int retry_attempts_{0}; // It is 1-based and represents the ordinal number of the next retry attempt (not completed attempts).

private:
    // State accessor
    inline State get_state_() const noexcept {
        return state_;
    }

    // State mutator with logging
    inline void set_state_(State new_state) noexcept {
        WK_TRACE("[CONN] State:  " << to_string(state_) << " -> " << to_string(new_state));
        state_ = new_state;
    }

    inline void enter_connected_state_() noexcept {
        // Update state
        set_state_(State::Connected);
        // Reset retry state
        retry_attempts_ = 0;
        retry_root_error_.store(Error::None, std::memory_order_relaxed);
        // Reset liveness tracking
        liveness_warning_fired_ = false;
        auto now = std::chrono::steady_clock::now();
        last_heartbeat_ts_.store(now, std::memory_order_relaxed);
        last_message_ts_.store(now, std::memory_order_relaxed);
        // Invoke connect callback (if any)
        if (hooks_.on_connect_cb_) {
            hooks_.on_connect_cb_();
        }
    }

    inline void recompute_liveness_windows_() noexcept {
        auto total = std::max(message_timeout_, heartbeat_timeout_);
        liveness_danger_window_ = total - std::chrono::milliseconds(static_cast<int>(total.count() * liveness_warning_ratio_));
    }

    [[nodiscard]]
    inline std::chrono::milliseconds liveness_remaining_() const noexcept {
        auto now = std::chrono::steady_clock::now();
        // last message liveness remaining
        auto last_msg = last_message_ts_.load(std::memory_order_relaxed);
        auto msg_left = message_timeout_ - std::chrono::duration_cast<std::chrono::milliseconds>(now - last_msg);
        // last heartbeat liveness remaining
        auto last_hb = last_heartbeat_ts_.load(std::memory_order_relaxed);
        auto hb_left = heartbeat_timeout_ - std::chrono::duration_cast<std::chrono::milliseconds>(now - last_hb);
        // return the maximum remaining time of both signals
        return std::max(msg_left, hb_left);
    }

    [[nodiscard]]
    inline bool is_liveness_stale_() noexcept {
        auto now = std::chrono::steady_clock::now();
        // last message liveness
        auto last_msg = last_message_ts_.load(std::memory_order_relaxed);
        bool message_stale   = (now - last_msg) > message_timeout_;
        // last heartbeat liveness
        auto last_hb  = last_heartbeat_ts_.load(std::memory_order_relaxed);
        bool heartbeat_stale = (now - last_hb) > heartbeat_timeout_;
        // Conservative: only true if BOTH are stale
        return message_stale && heartbeat_stale;
    }

    inline void create_transport_() {
        // If exists, ensure old transport is torn down deterministically
        if (ws_) {
            ws_->close();
            ws_.reset();
        }
        // Initialize transport
        ws_ = std::make_unique<WS>(telemetry_.websocket);
        // Set callbacks
        ws_->set_message_callback([this](std::string_view msg) {
            on_message_received_(msg);
        });
        ws_->set_error_callback([this](Error error) {
            on_transport_error_(error);
        });
        ws_->set_close_callback([this]() {
            on_transport_closed_();
        });
    }
                    
    // Placeholder for user-defined behavior on message receipt
    inline void on_message_received_(std::string_view msg) {
        // Set last message timestamp
        last_message_ts_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        // Reset liveness warning state
        liveness_warning_fired_ = false;
        if (hooks_.on_message_cb_) {
            WK_TL1( telemetry_.messages_forwarded_total.inc() ); // This marks handoff, not consumption
            hooks_.on_message_cb_(msg);
        }
    }

    // Placeholder for user-defined behavior on transport errors
    inline void on_transport_error_(Error error) {
        // Do NOT overwrite last error after a forced decision
        if (get_state_() == State::ForcedDisconnection) {
            return;
        }
        WK_WARN("[CONN] Transport error: " << to_string(error));
        last_error_.store(error, std::memory_order_relaxed);
        // Errors are handled by the receive loop and lead to closure
        // Higher-level protocols can observe closure via on_disconnect callback
    }

    // Placeholder for user-defined behavior on transport closure
    inline void on_transport_closed_() {
        // Guard against multiple invocations
        if (get_state_() == State::Disconnected) {
            return; // already resolved
        }
        // Guard against connect() never completing
        if (get_state_() == State::Connecting) { // connect never completed — this is not a disconnect event
            set_state_(State::Disconnected);
            return;
        }
        WK_TL1( telemetry_.disconnect_events_total.inc() ); // transport closure as observed by Connection
        WK_DEBUG("[CONN] WebSocket closed.");
        if (hooks_.on_disconnect_cb_) {
            hooks_.on_disconnect_cb_();
        }
        // Determine retry based on the current state and last error
        const auto error = last_error_.load(std::memory_order_relaxed);
        if ((get_state_() == State::Connected || get_state_() == State::ForcedDisconnection)
            && should_retry_(error)) {
            WK_TL1( telemetry_.retry_cycles_started_total.inc() ); // This counts retry cycles, not attempts
            set_state_(State::WaitingReconnect);
            // Retry immediately
            arm_immediate_reconnect_(error);
        }
        else {
            set_state_(State::Disconnected);
        }
    }

    // Determines whether a transport error represents a transient, external
    // failure that should trigger automatic reconnection. Caller misuse,
    // protocol violations, and intentional shutdowns are never retried.
    [[nodiscard]]
    inline bool should_retry_(Error error) const noexcept {
        switch (error) {
            // expected external conditions -> retry
            case Error::ConnectionFailed:
            case Error::HandshakeFailed:
            case Error::Timeout:
            case Error::RemoteClosed:
            // “unknown but bad” failure -> retry (conservative default)
            case Error::TransportFailure:
                WK_TRACE("[CONN] should retry after '" << to_string(error) << "'? -> YES");
                return true;

            // caller or logic errors -> no retry
            case Error::InvalidUrl:
            case Error::InvalidState:
            case Error::Cancelled:
            // protocol corruption -> no retry
            case Error::ProtocolError:
            // Explicit shutdown intent -> no retry
            case Error::LocalShutdown:
            default:
                WK_TRACE("[CONN] should retry after '" << to_string(error) << "'? -> NO");
                return false;
        }
    }

    [[nodiscard]]
    inline bool reconnect_() {
        WK_TL1( telemetry_.retry_attempts_total.inc() ); // One attempt = one call
        if (get_state_() != State::WaitingReconnect) {
            WK_WARN("[CONN] reconnect() called while not waiting to reconnect (state: " << to_string(get_state_()) << "). Ignoring.");
            return false;
        }
        WK_INFO("[CONN] Attempting reconnection... (attempt " << (retry_attempts_) << ")");
        // 1) Enter connecting state
        set_state_(State::Connecting);
        // 2) Parse and validate URL
        ParsedUrl parsed_url;
        auto error = parse_url(last_url_, parsed_url);
        if (error != Error::None) {
            WK_ERROR("[CONN] URL parsing failed");
            last_error_.store(error, std::memory_order_relaxed);
            set_state_(State::Disconnected);
            return false;
        }
        // 3) Create a fresh transport instance
        create_transport_();
        // 4) Attempt reconnection
        WK_INFO("[CONN] Reconnecting to: " << last_url_);
        error = ws_->connect(parsed_url.host, parsed_url.port, parsed_url.path);
        last_error_.store(error, std::memory_order_relaxed);
        if (error != Error::None) {
            WK_TL1( telemetry_.retry_failure_total.inc() ); // Failure = attempt did not connect
            set_state_(State::WaitingReconnect);
            WK_ERROR("[CONN] Reconnection failed.");
            return false;
        }
        // 5) Enter connected state
        WK_TL1( telemetry_.retry_success_total.inc() ); // State-based success, not transport-based
        WK_INFO("[CONN] Connection re-established with server '" << last_url_ << "'.");
        enter_connected_state_();
        return true;
    }

    // Schedule immediate retry (no backoff)
    void arm_immediate_reconnect_(Error error) noexcept {
        retry_root_error_.store(error, std::memory_order_relaxed);
        retry_attempts_ = 1;
        // next_retry_ intentionally not set, so the first retry is attempted immediately on next poll()
    }

    // Schedule next retry with backoff
    void schedule_next_retry_() noexcept {
        retry_attempts_++;
        auto now = std::chrono::steady_clock::now();
        auto delay = backoff_(retry_root_error_.load(std::memory_order_relaxed), retry_attempts_);
        next_retry_ = now + delay;
        // Invoke retry callback (if any)
        if (hooks_.on_retry_cb_) {
            hooks_.on_retry_cb_(RetryContext{
                .url         = last_url_,
                .error       = retry_root_error_.load(std::memory_order_relaxed),
                .attempt     = retry_attempts_,
                .next_delay  = delay
            });
        }
        // convert to seconds for logging
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(delay);
        if (seconds.count() == 1) {
            WK_INFO("[CONN] Next reconnection attempt in 1 second.");
        } else if (seconds.count() > 1) {
            WK_INFO("[CONN] Next reconnection attempt in " << seconds.count() << " seconds.");
        }
    }

    [[nodiscard]]
    inline std::chrono::milliseconds backoff_(Error error, int attempt) const noexcept {
        // Clamp exponent to avoid overflow / long stalls
        attempt = std::min(attempt, 6); // 100 * 2^6 = 6400ms → capped
        // Determine backoff based on error type
        switch (error) {
            // --- Fast retry ---------------------------------------------------
            case Error::RemoteClosed:
            case Error::Timeout: {
                constexpr std::chrono::milliseconds base{50};
                constexpr std::chrono::milliseconds max{1000};
                auto delay = base * (1 << attempt);
                return std::min(delay, max);
            }
            // --- Moderate retry -----------------------------------------------
            case Error::ConnectionFailed:
            case Error::HandshakeFailed: {
                constexpr std::chrono::milliseconds base{100};
                constexpr std::chrono::milliseconds max{5000};
                auto delay = base * (1 << attempt);
                return std::min(delay, max);
            }
            // --- Conservative retry -------------------------------------------
            case Error::TransportFailure: {
                constexpr std::chrono::milliseconds base{200};
                constexpr std::chrono::milliseconds max{10000};
                auto delay = base * (1 << attempt);
                return std::min(delay, max);
            }
            // --- Should never retry -------------------------------------------
            default:
                return std::chrono::milliseconds::max();
        }
    }
};

} // namespace transport
} // namespace wirekrak::core

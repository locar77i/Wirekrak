#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <chrono>
#include <memory>

#include "wirekrak/core/transport/concepts.hpp"
#include "wirekrak/core/transport/telemetry/connection.hpp"
#include "wirekrak/core/transport/parse_url.hpp"
#include "wirekrak/core/transport/state.hpp"
#include "wirekrak/core/transport/connection/signal.hpp"
#include "wirekrak/core/transport/websocket/events.hpp"
#include "wirekrak/core/transport/websocket/data_block.hpp"
#include "wirekrak/core/telemetry.hpp"
#include "lcr/optional.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak::core::transport {

/*
===============================================================================
 wirekrak::core::transport::Connection
===============================================================================

Generic transport-level connection abstraction, parameterized by a WebSocket
transport implementation conforming to transport::WebSocketConcept.

A Connection represents a *logical* connection whose identity remains stable
across transient transport failures and automatic reconnections.

This component encapsulates all *transport-level* concerns and is designed
to be reused across protocols (Kraken, future exchanges, custom feeds).

It is intentionally decoupled from any exchange schema or message format.

-------------------------------------------------------------------------------
 Responsibilities
-------------------------------------------------------------------------------
- Establish and manage a logical WebSocket connection
- Own the transport lifecycle (connect, disconnect, retry)
- Track transport progress and activity signals
- Detect liveness failure deterministically
- Expose only observable consequences via edge-triggered events

-------------------------------------------------------------------------------
 Progress & Observability Model
-------------------------------------------------------------------------------
The Connection exposes *facts*, not inferred health states:

- transport_epoch
    Incremented once per successful WebSocket connection.
    Represents completed transport lifetimes.

- rx_messages / tx_messages
    Monotonic counters tracking successfully received and sent messages.

- connection::Signal
    Edge-triggered, single-shot events for externally observable transitions
    (Connected, Disconnected, RetryScheduled, LivenessThreatened).

No level-based liveness or health state is exposed.

-------------------------------------------------------------------------------
 Liveness & Reconnection Semantics
-------------------------------------------------------------------------------
- Two independent activity signals are tracked:
    * Last received message timestamp
    * Last received heartbeat timestamp
- Liveness failure occurs only if BOTH signals are stale
- On liveness timeout:
    * The transport is force-closed
    * Normal reconnection logic applies
- Warning and timeout are edge-triggered and emitted at most once per silence window

-------------------------------------------------------------------------------
 Design Guarantees
-------------------------------------------------------------------------------
- No inheritance and no virtual functions
- Zero runtime polymorphism (concept-based design)
- Header-only, zero-cost abstractions
- Transport-agnostic via transport::WebSocketConcept
- Fully testable using mock transports
- No background threads; all logic is poll-driven

-------------------------------------------------------------------------------
 Usage Model
-------------------------------------------------------------------------------
- Call open(url) once to activate the connection
- Drive all progress by calling poll() regularly
- Observe progress via:
    * transport_epoch
    * rx/tx counters
    * connection::Signal edges
- Compose this Connection inside protocol-level sessions (e.g. Kraken)
- is_idle() reports current quiescence only; new external I/O may arrive
  immediately after it returns true.

-------------------------------------------------------------------------------
 Notes
-------------------------------------------------------------------------------
- URL parsing is intentionally minimal (ws:// and wss:// only)
- TLS is delegated to the underlying transport
- Designed for ultra-low-latency (ULL) and deterministic environments

===============================================================================
*/

constexpr auto HEARTBEAT_TIMEOUT = std::chrono::seconds(15); // default heartbeat timeout
constexpr auto MESSAGE_TIMEOUT   = std::chrono::seconds(15); // default message timeout
constexpr auto LIVENESS_WARNING_RATIO = 0.8;                 // warn when 80% of liveness window is elapsed

template <
    transport::WebSocketConcept WS,
    typename MessageRing
>
class Connection {
public:
    Connection(MessageRing& ring,
               telemetry::Connection& telemetry,
               std::chrono::seconds heartbeat_timeout = HEARTBEAT_TIMEOUT,
               std::chrono::seconds message_timeout = MESSAGE_TIMEOUT,
               double liveness_warning_ratio = LIVENESS_WARNING_RATIO) noexcept
        : message_ring_(ring)
        , telemetry_(telemetry)
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
        WK_DEBUG("[CONN] Connecting to: " << url);
        WK_TL1( telemetry_.open_calls_total.inc() ); // This represents explicit caller intent

        // --- Synchronous preconditions (must succeed before FSM starts) ---

        // 0) PRECONDITION: must be disconnected or waiting to reconnect
        if (get_state_() != State::Disconnected && get_state_() != State::WaitingReconnect) {
            WK_WARN("[CONN] open() called while not disconnected  (state: " << to_string(get_state_()) << "). Ignoring.");
            return Error::InvalidState;
        }
        // 1) PRECONDITION: parse and validate URL
        last_url_ = url;
        ParsedUrl tmp;
        last_error_ = parse_url(url, tmp);
        if (last_error_ != Error::None) {
            WK_ERROR("[CONN] URL parsing failed");
            return last_error_;
        }
        parsed_url_ = std::move(tmp);
        // 2) Enter FSM: all preconditions satisfied, begin connection attempt
        transition_(Event::OpenRequested);
        // 3) Create a fresh transport instance
        create_transport_();
        // 4) Attempt reconnection
        last_error_ = ws_->connect(parsed_url_.value().host, parsed_url_.value().port, parsed_url_.value().path);
        if (last_error_ != Error::None) {
            WK_ERROR("[CONN] Connection failed (" << to_string(last_error_) << ")");
            // Transport connection attempt failed (initial connect path)
            transition_(Event::TransportConnectFailed, last_error_);
            return last_error_;
        }
        // 5) Transport connection established -> finalize Connected state
        WK_TL1( telemetry_.connect_success_total.inc() ); // This reflects a state-machine fact, not a transport fact.
        transition_(Event::TransportConnected);
        WK_INFO("[CONN] Connected to server: " << last_url_);
        return Error::None;
    }

    // Manual disconnect - close() performs an unconditional shutdown and
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
        // User intent: request graceful shutdown of the logical connection
        transition_(Event::CloseRequested);
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
            // Update tx message count and timestamp
            ++tx_messages_;
            last_message_ts_ = std::chrono::steady_clock::now();
            return true;
        }
        return false;
    }

    // Event loop
    inline void poll() noexcept {
        //WK_TRACE("[CONN] Polling connection ... (state: " << to_string(get_state_()) << ")");
        // === Drain transport events ===
        websocket::Event ev;
        while (ws_->poll_event(ev)) {
            switch (ev.type) {
                case websocket::EventType::Close:
                    on_transport_closed_();
                    break;

                case websocket::EventType::Error:
                    on_transport_error_(ev.error);
                    break;
            }
        }
        // === Reconnection logic ===
        auto now = std::chrono::steady_clock::now();
        if (get_state_() == State::WaitingReconnect && now >= next_retry_) {
            (void)reconnect_();
        }
        // === Liveness logic ===
        // NOTE: liveness is evaluated only while Connected.
        // Once a timeout forces disconnection, reconnection logic takes over.
        if (get_state_() == State::Connected) {
            // === Liveness warning check ===
            auto remaining = liveness_remaining_();
            if (!liveness_warning_emitted_) [[likely]] {
                if (remaining <= liveness_danger_window_) {
                    WK_TRACE("[CONN] Liveness warning: " << remaining.count() << "ms remaining.");
                    liveness_warning_emitted_ = true;
                    transition_(Event::LivenessOutdated);
                }
            } else { // If liveness warning was previously emitted,
                // reset it once observable activity restores liveness above the danger window.
                if (remaining > liveness_danger_window_) {
                    liveness_warning_emitted_ = false;
                }
            }
            // === Liveness timeout check ===
            if (!liveness_timeout_emitted_ && is_liveness_stale_()) {
                WK_DEBUG("[CONN] Liveness timeout: No protocol traffic observed within liveness window (Forcing reconnect).");
                liveness_timeout_emitted_ = true;
                transition_(Event::LivenessExpired, Error::Timeout);
            }
        }
    }

    [[nodiscard]]
    inline bool poll_signal(connection::Signal& out) noexcept {
        return events_.pop(out);
    }

    // Returns an observable concept (not a state enum)
    [[nodiscard]]
    inline bool is_active() const noexcept {
        return state_ == State::Connected
            || state_ == State::Connecting
            || state_ == State::Disconnecting
            || state_ == State::WaitingReconnect
        ;
    }

    // Accessors
    [[nodiscard]]
    inline std::uint64_t epoch() const noexcept {
        return epoch_;
    }

    [[nodiscard]]
    inline std::uint64_t hb_messages() const noexcept {
        return heartbeat_total_;
    }

    [[nodiscard]]
    inline std::uint64_t& heartbeat_total() noexcept {
        return heartbeat_total_;
    }

    [[nodiscard]]
    inline const std::uint64_t& heartbeat_total() const noexcept {
        return heartbeat_total_;
    }

    [[nodiscard]]
    inline std::chrono::steady_clock::time_point& last_heartbeat_ts() noexcept {
        return last_heartbeat_ts_;
    }

    [[nodiscard]]
    inline const std::chrono::steady_clock::time_point& last_heartbeat_ts() const noexcept {
        return last_heartbeat_ts_;
    }

    [[nodiscard]]
    inline std::uint64_t rx_messages() const noexcept {
        return rx_messages_;
    }

    [[nodiscard]]
    inline std::uint64_t tx_messages() const noexcept {
        return tx_messages_;
    }

    [[nodiscard]]
    inline const std::chrono::steady_clock::time_point last_message_ts() const noexcept {
        return last_message_ts_;
    }

    // Mutators
    inline void set_liveness_timeout(std::chrono::milliseconds heartbeat, std::chrono::milliseconds message) noexcept {
        heartbeat_timeout_ = heartbeat;
        message_timeout_   = message;
        recompute_liveness_windows_();
    }

    // -----------------------------------------------------------------------------
    // Idle observation
    // -----------------------------------------------------------------------------
    //
    // Returns true if the connection is currently quiescent:
    //
    // - No pending connection::Signal events
    // - No reconnect timer ready to fire
    // - poll() would not advance state unless new I/O arrives
    //
    // This method:
    // - Does NOT call poll()
    // - Does NOT mutate state
    // - Does NOT perform I/O
    //
    // New external activity may arrive after this returns true.
    //
    [[nodiscard]]
    inline bool is_idle() const noexcept {
        // 1) Pending observable signals → not idle
        if (!events_.empty()) {
            return false;
        }

        // 2) Reconnect timer ready to fire → not idle
        if (get_state_() == State::WaitingReconnect) {
            auto now = std::chrono::steady_clock::now();
            if (next_retry_ != std::chrono::steady_clock::time_point{} && now >= next_retry_) {
                return false;
            }
        }

        // Otherwise, no work pending
        return true;
    }

    [[nodiscard]]
    inline websocket::DataBlock* peek_message() noexcept {
        assert(ws_ && "Transport not initialized");
        websocket::DataBlock* block = ws_->peek_message();
        if (block) { // Update rx message count and timestamp
            WK_TL1( telemetry_.messages_forwarded_total.inc() );
            ++rx_messages_;
            last_message_ts_ = std::chrono::steady_clock::now();
        }
        return block;
    }

    inline void release_message() noexcept {
        assert(ws_ && "Transport not initialized");
        ws_->release_message();
    }

#ifdef WK_UNIT_TEST
public:
    inline void force_last_message(std::chrono::steady_clock::time_point ts) noexcept{
        last_message_ts_ = ts;
    }

    inline void force_last_heartbeat(std::chrono::steady_clock::time_point ts) noexcept {
        last_heartbeat_ts_ = ts;
    }

    WS& ws() {
        return *ws_;
    }
#endif // WK_UNIT_TEST

private:
    std::string last_url_;                          // for logging / retry callbacks
    lcr::optional<ParsedUrl> parsed_url_;           // Invariant: parsed_url_.has() == true -> Valid endpoint

    // Data message queue (transport → connection/session)
    MessageRing& message_ring_;

    transport::telemetry::Connection& telemetry_;   // Telemetry reference (not owned)
    std::unique_ptr<WS> ws_;                        // WebSocket instance (owned by Connection)

    // Current transport epoch (incremented on each websocket connection: exposed progress signal.)
    std::uint64_t epoch_{0};

    // Heartbeat messages tracking (for liveness monitoring)
    std::uint64_t heartbeat_total_{0};
    std::chrono::steady_clock::time_point last_heartbeat_ts_{std::chrono::steady_clock::now()};

    // Message activity tracking (liveness and observability)
    std::uint64_t rx_messages_{0};
    std::uint64_t tx_messages_{0};
    std::chrono::steady_clock::time_point last_message_ts_{std::chrono::steady_clock::now()};

    // Liveness configuration
    std::chrono::milliseconds heartbeat_timeout_{10000};
    std::chrono::milliseconds message_timeout_{15000};
    double liveness_warning_ratio_;
    std::chrono::milliseconds liveness_danger_window_;

    // Liveness tracking state
    bool liveness_warning_emitted_{false};
    bool liveness_timeout_emitted_{false};

    // Error tracking for reconnection logic
    Error last_error_{Error::None};
    Error retry_root_error_{Error::None};
    DisconnectReason disconnect_reason_{DisconnectReason::None};

    // State machine 
    State state_{State::Disconnected};
    std::chrono::steady_clock::time_point next_retry_{};
    int retry_attempts_{0}; // It is 1-based and represents the ordinal number of the next retry attempt (not completed attempts).

    // The pending transition events
    lcr::lockfree::spsc_ring<connection::Signal, 16> events_;

    inline void emit_(connection::Signal sig) noexcept {
        WK_TRACE("[CONN] Emitting signal: " << to_string(sig));
        // Fast path: try to push
        if (events_.push(sig)) [[likely]] {
            return;
        }
        WK_WARN("[CONN] Failed to emit signal '" << to_string(sig) << "' (backpressure) - protocol correctness compromised (user is not draining fast enough)");
        // Future backpresusre policy (default:strict)
        // Wirekrak should never lie to the user or perform magic without explicit user instruction
        // Defensive action: close the connection to prevent further damage
        WK_FATAL("[CONN] Forcing connection close to preserve correctness guarantees.");
        close();
    }

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

    // State machine transition function
    inline void transition_(Event event, Error error = Error::None) noexcept {
        const State state  = get_state_();

        WK_TRACE("[FSM] (" << to_string(state) << ") --" << to_string(event) << "-->");

        switch (state) {

        // ================================================================
        case State::Disconnected:
            switch (event) {
            case Event::OpenRequested:
                set_state_(State::Connecting);
                break;

            default:
                break;
            }
            break;

        // ================================================================
        case State::Connecting:
            switch (event) {
            case Event::TransportConnected:
                // Transport connection established -> enter fully connected state
                set_state_(State::Connected);
                emit_(connection::Signal::Connected);
                // Reset retry state
                retry_attempts_ = 0;
                retry_root_error_ = Error::None;
                { // Reset liveness tracking
                    last_message_ts_ = last_heartbeat_ts_ = std::chrono::steady_clock::now();
                    liveness_warning_emitted_ = false;
                    liveness_timeout_emitted_ = false;
                }
                disconnect_reason_ = DisconnectReason::None;
                // Only increment on Connected (Never on retries, attempts, or disconnections)
                ++epoch_;
                break;

            case Event::TransportConnectFailed:
                WK_TL1( telemetry_.connect_failure_total.inc() );
                if (should_retry_(error)) {
                    WK_TL1( telemetry_.retry_cycles_started_total.inc() ); // This counts retry cycles, not attempts
                    set_state_(State::WaitingReconnect);
                    arm_immediate_reconnect_(error); // Retry immediately
                }
                else {
                    set_state_(State::Disconnected);
                    disconnect_reason_ = DisconnectReason::TransportError;
                }
                break;

            case Event::TransportReconnectFailed:
                // Reconnection attempt failed -> apply backoff-based retry policy
                WK_TL1( telemetry_.retry_failure_total.inc() );
                disconnect_reason_ = DisconnectReason::TransportError;
                if (should_retry_(error)) {
                    set_state_(State::WaitingReconnect);
                    schedule_next_retry_();  // Schedule next retry with backoff
                } else {
                    set_state_(State::Disconnected);
                }
                break;

            case Event::TransportClosed:
                // Transport closed before reaching Connected -> resolve to Disconnected
                set_state_(State::Disconnected);
                break;

            case Event::CloseRequested:
                set_state_(State::Disconnected);
                break;

            default:
                break;
            }
            break;

        // ================================================================
        case State::Connected:
            switch (event) {
            case Event::LivenessOutdated:
                emit_(connection::Signal::LivenessThreatened);
                break;
            case Event::LivenessExpired:
                WK_TL1( telemetry_.liveness_timeouts_total.inc() );
                last_error_ = error;
                disconnect_reason_ = DisconnectReason::LivenessTimeout;
                set_state_(State::Disconnecting);
                ws_->close(); // Force transport failure → triggers reconnection
                break;

            case Event::CloseRequested:
                WK_DEBUG("[CONN] Disconnecting from: " << last_url_);
                disconnect_reason_ = DisconnectReason::LocalClose;
                set_state_(State::Disconnecting);
                // This event does not cancel pending retry state explicitly;
                // retry resolution is handled by transport close callback.
                ws_->close();
                WK_INFO("[CONN] Disconnected from server: " << last_url_);
                break;

            case Event::TransportClosed:
                if (disconnect_reason_ != DisconnectReason::LocalClose && should_retry_(last_error_)) {
                    WK_TL1( telemetry_.retry_cycles_started_total.inc() ); // This counts retry cycles, not attempts
                    set_state_(State::WaitingReconnect);
                    arm_immediate_reconnect_(last_error_);
                } else {
                    set_state_(State::Disconnected);
                }
                break;

            default:
                break;
            }
            break;

        // ================================================================
        case State::Disconnecting:
            switch (event) {
            case Event::TransportClosed:
                if (disconnect_reason_ != DisconnectReason::LocalClose && should_retry_(last_error_)) {
                    set_state_(State::WaitingReconnect);
                    arm_immediate_reconnect_(last_error_);
                } else {
                    set_state_(State::Disconnected);
                }
                break;

            default:
                break;
            }
            break;

        // ================================================================
        case State::WaitingReconnect:
            switch (event) {
            case Event::RetryTimerExpired:
                set_state_(State::Connecting);
                break;

            case Event::OpenRequested:
                // Explicit open() overrides pending retry cycle
                set_state_(State::Connecting);
                break;

            case Event::CloseRequested:
                set_state_(State::Disconnected);
                break;

            default:
                break;
            }
            break;
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
        auto msg_left = message_timeout_ - std::chrono::duration_cast<std::chrono::milliseconds>(now - last_message_ts_);
        // last heartbeat liveness remaining
        auto hb_left = heartbeat_timeout_ - std::chrono::duration_cast<std::chrono::milliseconds>(now - last_heartbeat_ts_);
        // return the maximum remaining time of both signals
        return std::max(msg_left, hb_left);
    }

    [[nodiscard]]
    inline bool is_liveness_stale_() noexcept {
        auto now = std::chrono::steady_clock::now();
        // last message liveness
        bool message_stale   = (now - last_message_ts_) > message_timeout_;
        // last heartbeat liveness
        bool heartbeat_stale = (now - last_heartbeat_ts_) > heartbeat_timeout_;
        // Conservative: only true if BOTH are stale
        return message_stale && heartbeat_stale;
    }

    inline void create_transport_() {
        // If exists, ensure old transport is torn down deterministically
        if (ws_) {
            ws_->close();
            ws_.reset();
        }
        // Clear the message ring for the new epoch
        message_ring_.clear();
        // Initialize transport
        ws_ = std::make_unique<WS>(message_ring_, telemetry_.websocket);
    }

    // Placeholder for user-defined behavior on transport errors
    inline void on_transport_error_(Error error) {
        // Do not override an intentional disconnect decision
        if (disconnect_reason_ == DisconnectReason::LivenessTimeout || disconnect_reason_ == DisconnectReason::LocalClose) {
            return;
        }
        WK_WARN("[CONN] Transport error: " << to_string(error));
        last_error_ = error;
        disconnect_reason_ = DisconnectReason::TransportError;
    }

    // Placeholder for user-defined behavior on transport closure
    inline void on_transport_closed_() {
        // Guard against multiple invocations
        if (get_state_() == State::Disconnected) {
            return; // already resolved
        }
        // While Connecting, closure is resolved entirely by the FSM
        if (get_state_() == State::Connecting) {
            return;
        }
        WK_TL1( telemetry_.disconnect_events_total.inc() ); // transport closure as observed by Connection
        emit_(connection::Signal::Disconnected);
        // Notify FSM that the transport has closed (resolution is state-dependent)
        transition_(Event::TransportClosed, last_error_);
        WK_INFO("[CONN] Connection closed from server: " << last_url_ << " (reason: " << to_string(disconnect_reason_) << ")");
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
            case Error::Backpressure:
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
        WK_DEBUG("[CONN] Reconnecting to: " << last_url_ << " (attempt " << (retry_attempts_) << ")");
        WK_TL1( telemetry_.retry_attempts_total.inc() ); // One attempt = one call
        // 0) PRECONDITION: must be waiting to reconnect
        if (get_state_() != State::WaitingReconnect) {
            WK_WARN("[CONN] reconnect() called while not waiting to reconnect (state: " << to_string(get_state_()) << "). Ignoring.");
            return false;
        }
        // INVARIANT: parsed_url_ must be valid here
        assert(parsed_url_.has() && "reconnect cannot be called without the parsed url data");
        // 1) Retry delay elapsed -> FSM may initiate reconnection attempt
        transition_(Event::RetryTimerExpired);
        // 2) Create a fresh transport instance
        create_transport_();
        // 3) Attempt reconnection
        last_error_ = ws_->connect(parsed_url_.value().host, parsed_url_.value().port, parsed_url_.value().path);
        if (last_error_ != Error::None) {
            WK_ERROR("[CONN] Reconnection failed (" << to_string(last_error_) << ")");
            // Reconnection attempt failed -> apply backoff-based retry policy
            transition_(Event::TransportReconnectFailed, last_error_);
            return false;
        }
        // 4) Enter connected state
        WK_TL1( telemetry_.retry_success_total.inc() ); // State-based success, not transport-based
        transition_(Event::TransportConnected);
        WK_INFO("[CONN] Connection re-established with server '" << last_url_ << "'.");
        return true;
    }

    // Schedule immediate retry (no backoff)
    void arm_immediate_reconnect_(Error error) noexcept {
        WK_DEBUG("[CONN] Scheduling immediate reconnection attempt.");
        emit_(connection::Signal::RetryImmediate);
        retry_root_error_ = error;
        retry_attempts_ = 1;
        // next_retry_ intentionally not set, so the first retry is attempted immediately on next poll()
    }

    // Schedule next retry with backoff
    void schedule_next_retry_() noexcept {
        WK_DEBUG("[CONN] Scheduling next reconnection attempt with backoff.");
        emit_(connection::Signal::RetryScheduled);
        retry_attempts_++;
        auto now = std::chrono::steady_clock::now();
        auto delay = backoff_(retry_root_error_, retry_attempts_);
        next_retry_ = now + delay;
        WK_INFO("[CONN] Next reconnection attempt in " << delay.count() << " ms");
/*
        // convert to seconds for logging
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(delay);
        if (seconds.count() == 1) {
            WK_INFO("[CONN] Next reconnection attempt in 1 second.");
        } else if (seconds.count() > 1) {
            WK_INFO("[CONN] Next reconnection attempt in " << seconds.count() << " seconds.");
        }
*/
    }

    [[nodiscard]]
    inline std::chrono::milliseconds backoff_(Error error, int attempt) const noexcept {
        // Clamp exponent to avoid overflow / long stalls
        attempt = std::min(attempt, 6); // 100 * 2^6 = 6400ms → capped
        // Determine backoff based on error type
        switch (error) {
            // --- Fast retry ---------------------------------------------------
            case Error::RemoteClosed:
            case Error::Timeout: 
            case Error::Backpressure: 
            {
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

} // namespace wirekrak::core::transport

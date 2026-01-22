#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <atomic>
#include <chrono>
#include <memory>

#include "wirekrak/core/transport/concepts.hpp"
#include "wirekrak/core/transport/telemetry/websocket.hpp"
#include "wirekrak/core/transport/state.hpp"
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


template <transport::WebSocketConcept WS>
class Connection {
    static constexpr auto HEARTBEAT_TIMEOUT = std::chrono::seconds(15);
    static constexpr auto MESSAGE_TIMEOUT   = std::chrono::seconds(15);

public:
    using message_handler_t    = std::function<void(std::string_view)>;
    using connect_handler_t    = std::function<void()>;
    using disconnect_handler_t = std::function<void()>;
    using liveness_handler_t   = std::function<void()>;

public:
    Connection(std::chrono::seconds heartbeat_timeout = HEARTBEAT_TIMEOUT, std::chrono::seconds message_timeout = MESSAGE_TIMEOUT)
        : heartbeat_timeout_(heartbeat_timeout)
        ,  message_timeout_(message_timeout)
    {
    }

    // Ensure transport is closed on destruction.
    // Reconnection is not attempted after object lifetime ends.
    ~Connection() {
        close();
    }

    // Connection lifecycle
    [[nodiscard]]
    inline bool open(const std::string& url) noexcept {
        if (get_state_() != State::Disconnected && get_state_() != State::WaitingReconnect) {
            WK_WARN("[CONN] open() called while not disconnected  (state: " << to_string(get_state_()) << "). Ignoring.");
            return false;
        }
        last_url_ = url;
        // 1) Clear runtime state
        set_state_(State::Connecting);
        last_heartbeat_ts_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        last_message_ts_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        // 2) Create a fresh transport instance
        create_transport_();
        // 3) Attempt reconnection
        WK_INFO("[CONN] Connecting to: " << url);
        if (!parse_and_connect_(url)) {
            set_state_(State::Disconnected);
            WK_ERROR("[CONN] Connection failed.");
            return false;
        }
        // 4) Update state
        set_state_(State::Connected);
        retry_attempts_ = 0;
        WK_INFO("[CONN] Connected successfully.");
        if (hooks_.on_connect_cb_) {
            hooks_.on_connect_cb_();
        }
        return true;
    }

    // Manual disconnect (happy path)
    inline void close() noexcept {
        set_state_(State::Disconnecting);
        ws_->close();
    }

    // Sending
    [[nodiscard]]
    inline bool send(std::string_view text) noexcept {
        if (get_state_() != State::Connected) {
            WK_WARN("[CONN] send() called while not connected (state: " << to_string(get_state_()) << "). Ignoring.");
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
        // === Heartbeat liveness check ===
        if (get_state_() == State::Connected) {
            if (is_liveness_stale_()) {
                WK_TRACE("[CONN] Liveness timeout exceeded. Forcing reconnect.");
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
                retry_attempts_++;
                auto ms = backoff_(retry_attempts_);
                next_retry_ = now + ms;
                // convert to seconds for logging
                auto seconds = std::chrono::duration_cast<std::chrono::seconds>(ms);
                if (seconds.count() == 1) {
                    WK_INFO("[CONN] Next reconnection attempt in 1 second.");
                } else if (seconds.count() > 1) {
                    WK_INFO("[CONN] Next reconnection attempt in " << seconds.count() << " seconds.");
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

    void on_liveness_timeout(liveness_handler_t cb) noexcept {
        hooks_.on_liveness_timeout_cb_ = std::move(cb);
    }

    inline void set_liveness_timeout(std::chrono::milliseconds timeout) noexcept {
        heartbeat_timeout_ = timeout;
        message_timeout_ = timeout;
    }

    inline void set_liveness_timeout(std::chrono::milliseconds heartbeat_timeout, std::chrono::milliseconds message_timeout) noexcept {
        heartbeat_timeout_ = heartbeat_timeout;
        message_timeout_ = message_timeout;
    }

    // Accessors
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

#ifdef WK_UNIT_TEST
public:
    inline void force_last_message(std::chrono::steady_clock::time_point ts) noexcept{
        last_message_ts_.store(ts, std::memory_order_relaxed);
    }

    inline void force_last_heartbeat(std::chrono::steady_clock::time_point ts) noexcept {
        last_heartbeat_ts_.store(ts, std::memory_order_relaxed);
    }

    WS& ws() {
        return *ws_;
    }
#endif // WK_UNIT_TEST

private:
    std::string last_url_;
    transport::telemetry::WebSocket telemetry_;
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

    // Hooks structure to store all user-defined callbacks
    struct Hooks {
        message_handler_t    on_message_cb_{};
        connect_handler_t    on_connect_cb_{};
        disconnect_handler_t on_disconnect_cb_{};
        liveness_handler_t   on_liveness_timeout_cb_{};
    };

    // Handlers bundle
    Hooks hooks_;
    
    // State machine 
    State state_{State::Disconnected};
    std::chrono::steady_clock::time_point next_retry_{};
    int retry_attempts_{0};

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

    void create_transport_() {
        // Initialize transport
        ws_ = std::make_unique<WS>(telemetry_);
        // Set callbacks
        ws_->set_message_callback([this](std::string_view msg) {
            on_message_received_(msg);
        });
        ws_->set_close_callback([this]() {
            on_transport_closed_();
        });
    }
                    
    // Placeholder for user-defined behavior on message receipt
    inline void on_message_received_(std::string_view msg) {
        last_message_ts_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        if (hooks_.on_message_cb_) {
            hooks_.on_message_cb_(msg);
        }
    }

    // Placeholder for user-defined behavior on transport closure
    inline void on_transport_closed_() {
        WK_DEBUG("[CONN] WebSocket closed.");
        if (hooks_.on_disconnect_cb_) {
            hooks_.on_disconnect_cb_();
        }
        if (get_state_() == State::Connected || get_state_() == State::ForcedDisconnection) {
            set_state_(State::WaitingReconnect);
            retry_attempts_ = 1;
            next_retry_ = std::chrono::steady_clock::now(); // immediate retry
        }
        else {
            set_state_(State::Disconnected);
        }
    }

    // ---------------------------------------------------------------------
    // NOTE: Minimal URL parser supporting ws:// and wss://
    // Intentionally avoids allocations and regex.
    // Supports ws:// and wss:// only.
    //
    // Example inputs:
    //   wss://ws.kraken.com/v2
    //   ws://example.com:8080/stream
    // ---------------------------------------------------------------------
    [[nodiscard]]
    inline bool parse_and_connect_(const std::string& url) noexcept {
        // Parse URL into components -------------------------
        std::string scheme;
        std::string host;
        std::string port;
        std::string path;
        // 1) Extract scheme
        const std::string ws  = "ws://";
        const std::string wss = "wss://";
        size_t pos = 0;
        if (url.rfind(ws, 0) == 0) {
            scheme = "ws";
            pos = ws.size();
        }
        else if (url.rfind(wss, 0) == 0) {
            scheme = "wss";
            pos = wss.size();
        }
        else {
            return false;
        }
        // 2) Extract host[:port]
        size_t slash = url.find('/', pos);
        std::string hostport = (slash == std::string::npos) ? url.substr(pos) : url.substr(pos, slash - pos);
        // 3) Split host and port
        size_t colon = hostport.find(':');
        if (colon != std::string::npos) {
            host = hostport.substr(0, colon);
            port = hostport.substr(colon + 1);
        } else {
            host = hostport;
            port = (scheme == "wss") ? "443" : "80";
        }
        // 4) Path
        path = (slash == std::string::npos) ? "/" : url.substr(slash);
        // ---------------------------------------------------
        // 5) try connect
        return ws_->connect(host, port, path);
    }

    [[nodiscard]]
    inline bool reconnect_() {
        if (get_state_() != State::WaitingReconnect) {
            WK_WARN("[CONN] reconnect() called while not waiting to reconnect (state: " << to_string(get_state_()) << "). Ignoring.");
            return false;
        }
        WK_INFO("[CONN] Attempting reconnection... (attempt " << (retry_attempts_) << ")");
        // 1) Clear runtime state
        set_state_(State::Connecting);
        last_heartbeat_ts_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        last_message_ts_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        // 2) Recreate transport
        ws_.reset();          // DESTROY old WinHTTP stack
        create_transport_();  // fresh transport
        // 3) Attempt reconnection
        WK_INFO("[CONN] Reconnecting to: " << last_url_);
        if (!parse_and_connect_(last_url_)) {
            set_state_(State::WaitingReconnect);
            WK_ERROR("[CONN] Reconnection failed.");
            return false;
        }
        // 4) Set new state
        set_state_(State::Connected);
        retry_attempts_ = 0;
        auto now =  std::chrono::steady_clock::now();
        last_message_ts_.store(now, std::memory_order_relaxed);
        last_heartbeat_ts_.store(now, std::memory_order_relaxed);
        WK_INFO("[CONN] Connection re-established with server '" << last_url_ << "'.");
        // 4) Invoke connect callback
        if (hooks_.on_connect_cb_) {
            hooks_.on_connect_cb_();
        }
        return true;
    }

    inline std::chrono::milliseconds backoff_(int attempt) {
        constexpr std::chrono::milliseconds base{100};
        constexpr std::chrono::milliseconds max{5000};
        // Clamp exponent
        attempt = std::min(attempt, 6); // 100 * 2^6 = 6400ms → capped
        // Calculate delay
        auto delay = base * (1 << attempt);
        return std::min(delay, max);
    }
};

} // namespace transport
} // namespace wirekrak::core

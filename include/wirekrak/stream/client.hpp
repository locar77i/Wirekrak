#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <atomic>
#include <chrono>

#include "wirekrak/transport/concepts.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak {
namespace stream {

/*
===============================================================================
 wirekrak::stream::Client
===============================================================================

Generic streaming client template, parameterized by WebSocket transport
implementation conforming to transport::WebSocketConcept.

This component encapsulates all *connection-level* concerns and is designed
to be reused across protocols (Kraken, future exchanges, custom feeds).

It is intentionally decoupled from any exchange schema or message format.

-------------------------------------------------------------------------------
 Responsibilities
-------------------------------------------------------------------------------
- Establish and manage a WebSocket connection
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
- Subscriptions are replayed by higher-level protocol clients

-------------------------------------------------------------------------------
 Usage Model
-------------------------------------------------------------------------------
- Call connect(url) once
- Register callbacks (on_message, on_disconnect, on_liveness_timeout)
- Drive progress by calling poll() regularly
- Compose this client inside protocol-specific clients (e.g. Kraken)

-------------------------------------------------------------------------------
 Notes
-------------------------------------------------------------------------------
- URL parsing is intentionally minimal (ws:// and wss:// only)
- TLS is delegated to the underlying transport (e.g. WinHTTP + SChannel)
- This class is safe to unit-test without any real network access

===============================================================================
*/


template <transport::WebSocketConcept WS>
class Client {
    static constexpr auto HEARTBEAT_TIMEOUT = std::chrono::seconds(10);
    static constexpr auto MESSAGE_TIMEOUT   = std::chrono::seconds(15);

public:
    using message_handler_t    = std::function<void(std::string_view)>;
    using connect_handler_t    = std::function<void()>;
    using disconnect_handler_t = std::function<void()>;
    using liveness_handler_t   = std::function<void()>;

public:
    Client(std::chrono::seconds heartbeat_timeout = HEARTBEAT_TIMEOUT, std::chrono::seconds message_timeout = MESSAGE_TIMEOUT)
        : heartbeat_timeout_(heartbeat_timeout)
        ,  message_timeout_(message_timeout)
    {
        last_heartbeat_ts_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        last_message_ts_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        ws_.set_message_callback([this](const std::string& msg){
            on_message_received_(msg);
        });
        ws_.set_close_callback([this]() {
            on_transport_closed_();
        });
    }

    ~Client() {
        ws_.close();
    }

    // Connection lifecycle
    [[nodiscard]]
    inline bool connect(const std::string& url) noexcept {
        last_url_ = url;
        state_ = ConnState::Connecting;
    
        WK_INFO("Connecting to: " << url);
        if (!parse_and_connect_(url)) {
            state_ = ConnState::Disconnected;
            WK_ERROR("Connection failed.");
            return false;
        }
        state_ = ConnState::Connected;
        retry_attempts_ = 0;
        WK_INFO("Connected successfully.");
        if (hooks_.on_connect_cb_) {
            hooks_.on_connect_cb_();
        }
        return true;
    }

    inline void close() noexcept {
        ws_.close();
    }

    // Sending
    [[nodiscard]]
    inline bool send(std::string_view text) noexcept {
        // TODO: temporal implementation pseudocode
        if (ws_.send(std::string(text))) {
            last_message_ts_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    // // Event loop
    inline void poll() noexcept {
        auto now = std::chrono::steady_clock::now();
        // === Heartbeat liveness check ===
        if (state_ == ConnState::Connected) {
            auto last_msg = last_message_ts_.load(std::memory_order_relaxed);
            bool message_stale   = (now - last_msg) > message_timeout_;
            auto last_hb  = last_heartbeat_ts_.load(std::memory_order_relaxed);
            bool heartbeat_stale = (now - last_hb) > heartbeat_timeout_;
            // Conservative: only reconnect if BOTH are stale
            if (message_stale && heartbeat_stale) {
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_hb);
                WK_WARN("Heartbeat timeout (" << duration.count() << " ms). Forcing reconnect.");
                if (hooks_.on_liveness_timeout_cb_) {
                    hooks_.on_liveness_timeout_cb_();
                }
                // Force transport failure → triggers reconnection
                ws_.close();
            }
        }
        // === Reconnection logic ===
        if (state_ == ConnState::WaitingReconnect && now >= next_retry_) {
            WK_INFO("Attempting reconnection...");
            if (reconnect_()) {
                WK_INFO("Reconnected successfully");
                state_ = ConnState::Connected;
            } else {
                retry_attempts_++;
                next_retry_ = now + backoff_(retry_attempts_);
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
        return ws_;
    }
#endif // WK_UNIT_TEST

private:
    std::string last_url_;
    WS ws_;

    // The kraken heartbeats count is used as deterministic liveness signal that drives reconnection.
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
    
private:
    enum class ConnState {
        Disconnected,
        Connecting,
        Connected,
        WaitingReconnect
    };

    ConnState state_ = ConnState::Disconnected;
    std::chrono::steady_clock::time_point next_retry_;
    int retry_attempts_ = 0;

private:
    // Placeholder for user-defined behavior on message receipt
    inline void on_message_received_(std::string_view msg) {
        last_message_ts_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        if (hooks_.on_message_cb_) {
            hooks_.on_message_cb_(msg);
        }
    }

    // Placeholder for user-defined behavior on transport closure
    inline void on_transport_closed_() {
        WK_DEBUG("WebSocket closed.");
        if (hooks_.on_disconnect_cb_) {
            hooks_.on_disconnect_cb_();
        }
        if (state_ == ConnState::Connected) {
            state_ = ConnState::WaitingReconnect;
            retry_attempts_++;
            next_retry_ = std::chrono::steady_clock::now() + backoff_(retry_attempts_);
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
        return ws_.connect(host, port, path);
    }

    [[nodiscard]]
    inline bool reconnect_() {
        // 1) Close old WS
        ws_.close();
        // 2) Clear runtime state
        state_ = ConnState::Connecting;
        // 3) Attempt reconnection
        WK_INFO("Reconnecting to: " << last_url_);
        if (!parse_and_connect_(last_url_)) {
            state_ = ConnState::Disconnected;
            WK_ERROR("Reconnection failed.");
            return false;
        }
        state_ = ConnState::Connected;
        retry_attempts_ = 0;
        WK_INFO("Connection re-established with server '" << last_url_ << "'.");
        if (hooks_.on_connect_cb_) {
            hooks_.on_connect_cb_();
        }
        return true;
    }

    inline std::chrono::milliseconds backoff_(int attempt) {
        using namespace std::chrono;
        return std::min(
            milliseconds(100 * (1 << attempt)),
            milliseconds(5000)
        );
    }
};

} // namespace stream
} // namespace wirekrak

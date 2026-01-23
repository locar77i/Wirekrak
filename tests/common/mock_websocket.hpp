#pragma once

#include <string>
#include <functional>
#include <utility>
#include <concepts>

#include "wirekrak/core/transport/concepts.hpp"
#include "wirekrak/core/transport/error.hpp"
#include "wirekrak/core/transport/telemetry/websocket.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak::core::transport::test {

// NOTE:
// MockWebSocket uses process-global static state by design.
// This is intentional: transport::Connection owns a single-shot
// WebSocket instance internally and tests are strictly single-threaded.
// Each test MUST call MockWebSocket::reset() before constructing Connection.
class MockWebSocket {
    using MessageCallback = std::function<void(const std::string&)>;
    using CloseCallback   = std::function<void()>;
    using ErrorCallback   = std::function<void(Error)>;

    MessageCallback on_message_cb_;
    CloseCallback   on_close_cb_;
    ErrorCallback   on_error_cb_;

public:
    MockWebSocket(telemetry::WebSocket& telemetry) noexcept {
        (void)telemetry;
        WK_DEBUG("[MockWebSocket] constructed");
    }

    ~MockWebSocket() {
        WK_DEBUG("[MockWebSocket] destructed");
        
    }

    // ---------------------------------------------------------------------
    // transport::WebSocket API
    // ---------------------------------------------------------------------

    inline Error connect(const std::string&, const std::string&, const std::string&) noexcept {
        WK_DEBUG("[MockWebSocket] connect() called");
        connected_ = (next_connect_result_ == Error::None);
        return next_connect_result_;
    }

    inline bool send(const std::string&) noexcept {
        WK_DEBUG("[MockWebSocket] send() called");
        return connected_;
    }

    inline void close() noexcept {
        WK_DEBUG("[MockWebSocket] close() called");
        if (!connected_) return;
        connected_ = false;
        close_count_++;
        if (on_close_cb_) {
            on_close_cb_();
        }
    }

    inline void set_message_callback(MessageCallback cb) noexcept {
        on_message_cb_ = std::move(cb);
    }

    inline void set_close_callback(CloseCallback cb) noexcept {
        on_close_cb_ = std::move(cb);
    }

    inline void set_error_callback(ErrorCallback cb) noexcept {
        on_error_cb_ = std::move(cb);
    }

    // ---------------------------------------------------------------------
    // Test helpers
    // ---------------------------------------------------------------------

    inline void emit_message(const std::string& msg) noexcept {
        if (on_message_cb_) {
            on_message_cb_(msg);
        }
    }

    inline void emit_error(Error code = Error::TransportFailure) noexcept {
        error_count_++;
        if (on_error_cb_) {
            on_error_cb_(code);
        }
    }

    // Accessors
    static inline bool is_connected() noexcept {
        return connected_;
    }
    static inline int close_count() noexcept {
        return close_count_;
    }
    static inline int error_count() noexcept {
        return error_count_;
    }

    // mutators
    static inline void set_next_connect_result(Error err) noexcept {
        next_connect_result_ = err;
    }

    static inline void reset() noexcept {
        connected_   = false;
        close_count_ = 0;
        error_count_ = 0;
        next_connect_result_ = Error::None;
    }

private:
    // NOTE: Static state is intentional (single-shot WebSocket semantics)
    static inline bool connected_{false};
    static inline int close_count_{0};
    static inline int error_count_{0};
    static inline Error next_connect_result_{Error::None};
};
// Assert that MockWebSocket conforms to transport::WebSocketConcept concept
static_assert(WebSocketConcept<MockWebSocket>);

// ininitialize static members
} // namespace wirekrak::core::transport::test

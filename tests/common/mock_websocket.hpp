#pragma once

#include <string>
#include <functional>
#include <utility>
#include <concepts>

#include "wirekrak/core/transport/concepts.hpp"
#include "wirekrak/core/transport/telemetry/websocket.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak::core {
namespace transport {

class MockWebSocket {
    using MessageCallback = std::function<void(const std::string&)>;
    using CloseCallback   = std::function<void()>;
    using ErrorCallback   = std::function<void(unsigned long)>;

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

    inline bool connect(const std::string&, const std::string&, const std::string&) noexcept {
        WK_DEBUG("[MockWebSocket] connect() called");
        connected_ = true;
        return true;
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

    inline void emit_error(unsigned long code = 1) noexcept {
        error_count_++;
        if (on_error_cb_) {
            on_error_cb_(code);
        }
    }

    // Accessors
    inline bool is_connected() const noexcept {
        return connected_;
    }
    inline int close_count() const noexcept {
        return close_count_;
    }
    inline int error_count() const noexcept {
        return error_count_;
    }

    // mutators
    static inline void reset() noexcept {
        connected_   = false;
        close_count_ = 0;
        error_count_ = 0;
    }

private:
    static bool connected_;
    static int  close_count_;
    static int  error_count_;

};
// Assert that MockWebSocket conforms to transport::WebSocketConcept concept
static_assert(wirekrak::core::transport::WebSocketConcept<MockWebSocket>);

// ininitialize static members
bool MockWebSocket::connected_   = false;
int  MockWebSocket::close_count_ = 0;
int  MockWebSocket::error_count_ = 0;

} // namespace transport
} // namespace wirekrak::core

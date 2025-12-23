#pragma once

#include <string>
#include <functional>
#include <utility>
#include <concepts>

#include "wirekrak/transport/concepts.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak {
namespace transport {

class MockWebSocket {
    using MessageCallback = std::function<void(const std::string&)>;
    using CloseCallback   = std::function<void()>;
    using ErrorCallback   = std::function<void(unsigned long)>;

    MessageCallback on_message_cb_;
    CloseCallback   on_close_cb_;
    ErrorCallback   on_error_cb_;

public:
    MockWebSocket() {
        WK_DEBUG("[MockWebSocket] constructed");
    }

    ~MockWebSocket() {
        WK_DEBUG("[MockWebSocket] destructed");
    }

    // ---------------------------------------------------------------------
    // transport::WebSocket API
    // ---------------------------------------------------------------------

    inline bool connect(const std::string&, const std::string&, const std::string&) {
        WK_DEBUG("[MockWebSocket] connect() called");
        connected_ = true;
        return true;
    }

    inline bool send(const std::string&) {
        WK_DEBUG("[MockWebSocket] send() called");
        return connected_;
    }

    inline void close() {
        WK_DEBUG("[MockWebSocket] close() called");
        if (!connected_) return;
        connected_ = false;
        close_count_++;
        if (on_close_cb_) {
            on_close_cb_();
        }
    }

    inline void set_message_callback(MessageCallback cb) {
        on_message_cb_ = std::move(cb);
    }

    inline void set_close_callback(CloseCallback cb) {
        on_close_cb_ = std::move(cb);
    }

    inline void set_error_callback(ErrorCallback cb) {
        on_error_cb_ = std::move(cb);
    }

    // ---------------------------------------------------------------------
    // Test helpers
    // ---------------------------------------------------------------------

    inline void emit_message(const std::string& msg) {
        if (on_message_cb_) {
            on_message_cb_(msg);
        }
    }

    inline void emit_error(unsigned long code = 1) {
        error_count_++;
        if (on_error_cb_) {
            on_error_cb_(code);
        }
    }

    // Accessors
    inline bool is_connected() const {
        return connected_;
    }
    inline int close_count() const {
        return close_count_;
    }
    inline int error_count() const {
        return error_count_;
    }

    // mutators
    static inline void reset() {
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
static_assert(wirekrak::transport::WebSocketConcept<MockWebSocket>);

// ininitialize static members
bool MockWebSocket::connected_   = false;
int  MockWebSocket::close_count_ = 0;
int  MockWebSocket::error_count_ = 0;

} // namespace transport
} // namespace wirekrak

#pragma once

#include <string>
#include <functional>
#include <utility>
#include <concepts>

#include "wirekrak/transport/concepts.hpp"


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

    // ---------------------------------------------------------------------
    // transport::WebSocket API
    // ---------------------------------------------------------------------

    bool connect(const std::string&, const std::string&, const std::string&) {
        connected_ = true;
        return true;
    }

    bool send(const std::string&) {
        return connected_;
    }

    void close() {
        if (!connected_) return;
        connected_ = false;
        close_count_++;
        if (on_close_cb_) {
            on_close_cb_();
        }
    }

    void set_message_callback(MessageCallback cb) {
        on_message_cb_ = std::move(cb);
    }

    void set_close_callback(CloseCallback cb) {
        on_close_cb_ = std::move(cb);
    }

    void set_error_callback(ErrorCallback cb) {
        on_error_cb_ = std::move(cb);
    }

    // ---------------------------------------------------------------------
    // Test helpers
    // ---------------------------------------------------------------------

    void emit_message(const std::string& msg) {
        if (on_message_cb_) {
            on_message_cb_(msg);
        }
    }

    void emit_error(unsigned long code = 1) {
        error_count_++;
        if (on_error_cb_) {
            on_error_cb_(code);
        }
    }

    // Accessors
    bool is_connected() const {
        return connected_;
    }
    int close_count() const {
        return close_count_;
    }
    int error_count() const {
        return error_count_;
    }

private:
    bool connected_   = false;
    int  close_count_ = 0;
    int  error_count_ = 0;

};
// Assert that MockWebSocket conforms to transport::WebSocketConcept concept
static_assert(wirekrak::transport::WebSocketConcept<MockWebSocket>);

} // namespace transport
} // namespace wirekrak

#pragma once

#include <string>
#include <functional>
#include <utility>
#include <concepts>
#include <cstring>

#include "wirekrak/core/transport/concepts.hpp"
#include "wirekrak/core/transport/error.hpp"
#include "wirekrak/core/transport/telemetry/websocket.hpp"
#include "wirekrak/core/transport/websocket/events.hpp"
#include "wirekrak/core/transport/websocket/data_block.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak::core::transport::test {

constexpr static std::size_t RX_RING_CAPACITY = 8; // Capacity of the message ring buffer (number of messages)


// NOTE:
// MockWebSocket uses process-global static state by design.
// This is intentional: transport::Connection owns a single-shot
// WebSocket instance internally and tests are strictly single-threaded.
// Each test MUST call MockWebSocket::reset() before constructing Connection.
class MockWebSocket {
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
        if (!control_ring_.push(websocket::Event::make_close())) {
            handle_control_ring_full_();
        }
    }

    [[nodiscard]]
    inline bool poll_event(websocket::Event& out) noexcept {
        return control_ring_.pop(out);
    }

    // ---------------------------------------------------------------------
    // Test helpers
    // ---------------------------------------------------------------------

    inline void emit_message(const std::string& msg) noexcept {
        websocket::DataBlock* block = message_ring_.acquire_producer_slot();
        if (!block) {
            WK_WARN("[MockWebSocket] Message ring is full! Cannot emit message.");
            return;
        }
        std::strncpy(block->data, msg.data(), websocket::RX_BUFFER_SIZE);
        block->size = static_cast<uint32_t>(std::min(msg.size(), static_cast<size_t>(websocket::RX_BUFFER_SIZE)));
        message_ring_.commit_producer_slot();
    }

    inline void emit_error(Error error = Error::TransportFailure) noexcept {
        error_count_++;
        if (!control_ring_.push(websocket::Event::make_error(error))) {
            handle_control_ring_full_();
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
        websocket::Event tmp;
        while (control_ring_.pop(tmp)) {}
        while (message_ring_.peek_consumer_slot() != nullptr) {
            message_ring_.release_consumer_slot();
        }
    }

     [[nodiscard]]
    inline websocket::DataBlock* peek_message() noexcept {
        return message_ring_.peek_consumer_slot();
    }

    inline void release_message() noexcept {
        message_ring_.release_consumer_slot();
    }

private:
    inline void handle_control_ring_full_() noexcept {
        WK_FATAL("[WS] Control event ring is full! Events may be lost.");
    }

private:
    // NOTE: Static state is intentional (single-shot WebSocket semantics)
    static inline bool connected_{false};
    static inline int close_count_{0};
    static inline int error_count_{0};
    static inline Error next_connect_result_{Error::None};

    // Control event queue (for signaling events like close and error)
    static inline lcr::lockfree::spsc_ring<websocket::Event, 16> control_ring_;

    // Data message queue (transport → connection/session)
    static inline lcr::lockfree::spsc_ring<websocket::DataBlock, RX_RING_CAPACITY> message_ring_;
};
// Assert that MockWebSocket conforms to transport::WebSocketConcept concept
static_assert(WebSocketConcept<MockWebSocket>);

// ininitialize static members
} // namespace wirekrak::core::transport::test

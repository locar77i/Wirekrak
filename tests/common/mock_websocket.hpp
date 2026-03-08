#pragma once

#include <string>
#include <functional>
#include <utility>
#include <concepts>
#include <cstring>

#include "wirekrak/core/transport/error.hpp"
#include "wirekrak/core/transport/telemetry/websocket.hpp"
#include "wirekrak/core/transport/websocket/events.hpp"
#include "lcr/buffer/concepts.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak::core::transport::test {

// NOTE:
// MockWebSocket uses process-global static state by design.
// This is intentional: transport::Connection owns a single-shot
// WebSocket instance internally and tests are strictly single-threaded.
// Each test MUST call MockWebSocket::reset() before constructing Connection.
template<
    typename ControlRing,
    lcr::buffer::ProducerSpscRingConcept MessageRing
>
class MockWebSocket {
public:
    MockWebSocket(ControlRing& control_ring, MessageRing& message_ring, telemetry::WebSocket& telemetry) noexcept
        : control_ring_(control_ring)
        , message_ring_(message_ring) {
        (void)telemetry;
        WK_DEBUG("[MockWebSocket] constructed");
    }

    ~MockWebSocket() {
        WK_DEBUG("[MockWebSocket] destructed");
        
    }

    // ---------------------------------------------------------------------
    // transport::WebSocket API
    // ---------------------------------------------------------------------

    inline Error connect(std::string_view, std::uint16_t, std::string_view, bool) noexcept {
        WK_DEBUG("[MockWebSocket] connect() called");
        connected_ = (next_connect_result_ == Error::None);
        return next_connect_result_;
    }

    inline bool send(std::string_view msg) noexcept {
        WK_DEBUG("[MockWebSocket] send() called: " << msg);
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

    // ---------------------------------------------------------------------
    // Test helpers
    // ---------------------------------------------------------------------

    inline void emit_message(const std::string& msg) noexcept {
        auto* slot = message_ring_.acquire_producer_slot();
        if (!slot) {
            WK_WARN("[MockWebSocket] Message ring is full! Cannot emit message.");
            return;
        }
        
        using promotion_result_type = typename MessageRing::promotion_result_type;
        promotion_result_type r = message_ring_.reserve(slot, msg.size());
        if (r > promotion_result_type::Success) {
            WK_FATAL("[MockWebSocket] Failed to reserve slot for mock message");
            return;
        }

        std::memcpy(slot->write_ptr(), msg.data(), msg.size());
        slot->commit(msg.size());

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
    ControlRing& control_ring_;

    // Data message queue (transport → connection/session)
    MessageRing& message_ring_;
};

} // namespace wirekrak::core::transport::test

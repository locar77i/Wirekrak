#pragma once


#include <cstdint>
#include <cstring>

#include "flashstrike/types.hpp"


namespace flashstrike {
namespace event {

// For live processing in matching engine. Why 64B?
// - Exactly one cache line per event → no false sharing.
// - Perfect for ring buffers / lock-free queues.
// - Critical path is ultra-predictable.
// - event_id first for WAL/replay ordering.
struct alignas(64) Request {
    EventId event_id;     // 8B - strictly increasing, replay anchor
    Timestamp timestamp;  // 8B - trading time (ns since epoch, or exchange clock)
    Price price;          // 8B
    Quantity quantity;    // 8B
    UserId user_id;       // 4B
    OrderId order_id;     // 4B
    RequestType type;    // 1B
    OrderType order_type; // 1B
    Side side;            // 1B
    uint8_t pad_[21];     // 21B padding to align to 64 bytes
    // ---------------------------------------------------------------------------
    inline void reset() noexcept {
        std::memset(this, 0, sizeof(Request));
    }

    inline void reset_pad() noexcept {
        std::memset(pad_, 0, sizeof(pad_));
    }
};
// Ensure layout correctness
static_assert(alignof(Request) == 64);
static_assert(sizeof(Request) == 64, "Request must be 64 bytes");
static_assert(offsetof(Request, event_id) == 0, "event_id offset mismatch");
static_assert(offsetof(Request, timestamp) == 8, "timestamp offset mismatch");
static_assert(offsetof(Request, price) == 16, "price offset mismatch");
static_assert(offsetof(Request, quantity) == 24, "quantity offset mismatch");
static_assert(offsetof(Request, user_id) == 32, "user_id offset mismatch");
static_assert(offsetof(Request, order_id) == 36, "order_id offset mismatch");
static_assert(offsetof(Request, type) == 40, "type offset mismatch");
static_assert(offsetof(Request, order_type) == 41, "order_type offset mismatch");
static_assert(offsetof(Request, side) == 42, "side offset mismatch");
// Ensure POD semantics
static_assert(std::is_standard_layout<Request>::value, "Request must have standard layout");
static_assert(std::is_trivial_v<Request>, "Request must be trivial");
static_assert(std::is_trivially_copyable_v<Request>, "Request must be trivially copyable");

} // namespace event
} // namespace flashstrike

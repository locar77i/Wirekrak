#pragma once

#include <chrono>

#include "wirekrak/core/protocol/config.hpp"
#include "wirekrak/core/protocol/kraken/schema/rejection_notice.hpp"
#include "wirekrak/core/protocol/kraken/schema/system/pong.hpp"
#include "wirekrak/core/protocol/kraken/schema/status/update.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/response.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/subscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/unsubscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/response.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/subscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/unsubscribe_ack.hpp"
#include "lcr/local/ring.hpp"


namespace wirekrak::core::protocol::kraken {

/*
===============================================================================
Context (OWNING STORE)
===============================================================================

Owns all parser-visible state:
- Output rings
- Heartbeat statistics
- Shared timestamps

The Context lifetime is controlled by the client.
Parsers NEVER own this object — they only receive ContextView.
===============================================================================
*/
struct Context {
    // Heartbeat statistics
    std::uint64_t& heartbeat_total;
    std::chrono::steady_clock::time_point& last_heartbeat_ts;

    // Last pong message
    lcr::optional<schema::system::Pong> pong_slot{};

    // Last status message
    lcr::optional<schema::status::Update> status_slot{};

    // Output rings for rejection notices
    lcr::local::ring<schema::rejection::Notice, config::REJECTION_RING_CAPACITY> rejection_ring{};

    // Output rings for trade channel
    lcr::local::ring<schema::trade::Response, config::TRADE_RING_CAPACITY> trade_ring{};
    lcr::local::ring<schema::trade::SubscribeAck, config::ACK_RING_CAPACITY> trade_subscribe_ring{};
    lcr::local::ring<schema::trade::UnsubscribeAck, config::ACK_RING_CAPACITY> trade_unsubscribe_ring{};

    // Output rings for book channel
    lcr::local::ring<schema::book::Response, config::BOOK_RING_CAPACITY> book_ring{};
    lcr::local::ring<schema::book::SubscribeAck, config::ACK_RING_CAPACITY> book_subscribe_ring{};
    lcr::local::ring<schema::book::UnsubscribeAck, config::ACK_RING_CAPACITY> book_unsubscribe_ring{};

    // ------------------------------------------------------------
    // Construction from owning Context
    // ------------------------------------------------------------
    explicit Context(std::uint64_t& hb_total, std::chrono::steady_clock::time_point& last_hb_ts) noexcept
        : heartbeat_total(hb_total)
        , last_heartbeat_ts(last_hb_ts)
    {}

    // Helper to check if all rings are empty
    [[nodiscard]]
    inline bool empty() const noexcept {
        return
            rejection_ring.empty() &&
            trade_ring.empty() &&
            trade_subscribe_ring.empty() &&
            trade_unsubscribe_ring.empty() &&
            book_ring.empty() &&
            book_subscribe_ring.empty() &&
            book_unsubscribe_ring.empty();
    }
};



/*
===============================================================================
Parser ContextView (Non-owning)
===============================================================================

Lightweight, non-nullable view over Context.
Passed to parsers and routers.

- No ownership
- No heap
- No null checks
- Enforced validity at construction
===============================================================================
*/
struct ContextView {
    // Heartbeat statistics
    std::uint64_t& heartbeat_total;
    std::chrono::steady_clock::time_point& last_heartbeat_ts;

    // Last pong message
    lcr::optional<schema::system::Pong>& pong_slot;

    // Last status message
    lcr::optional<schema::status::Update>& status_slot;

    // Output rings for rejection notices
    lcr::local::ring<schema::rejection::Notice, config::REJECTION_RING_CAPACITY>& rejection_ring;

    // Output rings for trade channel
    lcr::local::ring<schema::trade::Response, config::TRADE_RING_CAPACITY>& trade_ring;
    lcr::local::ring<schema::trade::SubscribeAck, config::ACK_RING_CAPACITY>& trade_subscribe_ring;
    lcr::local::ring<schema::trade::UnsubscribeAck, config::ACK_RING_CAPACITY>& trade_unsubscribe_ring;

    // Output rings for book channel
    lcr::local::ring<schema::book::Response, config::BOOK_RING_CAPACITY>& book_ring;
    lcr::local::ring<schema::book::SubscribeAck, config::ACK_RING_CAPACITY>& book_subscribe_ring;
    lcr::local::ring<schema::book::UnsubscribeAck, config::ACK_RING_CAPACITY>& book_unsubscribe_ring;

    // ------------------------------------------------------------
    // Construction from owning Context
    // ------------------------------------------------------------
    explicit ContextView(Context& ctx) noexcept
        : heartbeat_total(ctx.heartbeat_total)
        , last_heartbeat_ts(ctx.last_heartbeat_ts)
        , pong_slot(ctx.pong_slot)
        , status_slot(ctx.status_slot)
        , rejection_ring(ctx.rejection_ring)
        , trade_ring(ctx.trade_ring)
        , trade_subscribe_ring(ctx.trade_subscribe_ring)
        , trade_unsubscribe_ring(ctx.trade_unsubscribe_ring)
        , book_ring(ctx.book_ring)
        , book_subscribe_ring(ctx.book_subscribe_ring)
        , book_unsubscribe_ring(ctx.book_unsubscribe_ring)
    {}
};


} // namespace wirekrak::core::protocol::kraken

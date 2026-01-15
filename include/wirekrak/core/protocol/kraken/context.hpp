#pragma once

#include <atomic>
#include <chrono>

#include "wirekrak/core/config/ring_sizes.hpp"
#include "wirekrak/core/protocol/kraken/schema/rejection_notice.hpp"
#include "wirekrak/core/protocol/kraken/schema/system/pong.hpp"
#include "wirekrak/core/protocol/kraken/schema/status/update.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/response.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/subscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/unsubscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/response.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/subscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/unsubscribe_ack.hpp"

#include "lcr/lockfree/spsc_ring.hpp"

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
    // Heartbeat statistics
    std::atomic<uint64_t>& heartbeat_total;
    std::atomic<std::chrono::steady_clock::time_point>& last_heartbeat_ts;

    // Output rings for pong messages
    lcr::lockfree::spsc_ring<schema::system::Pong, config::pong_ring> pong_ring{};

    // Output rings for status channel
    lcr::lockfree::spsc_ring<schema::status::Update, config::status_ring> status_ring{};

    // Output rings for rejection notices
    lcr::lockfree::spsc_ring<schema::rejection::Notice, config::rejection_ring> rejection_ring{};

    // Output rings for trade channel
    lcr::lockfree::spsc_ring<schema::trade::Response, config::trade_update_ring> trade_ring{};
    lcr::lockfree::spsc_ring<schema::trade::SubscribeAck, config::subscribe_ack_ring> trade_subscribe_ring{};
    lcr::lockfree::spsc_ring<schema::trade::UnsubscribeAck, config::unsubscribe_ack_ring> trade_unsubscribe_ring{};

    // Output rings for book channel
    lcr::lockfree::spsc_ring<schema::book::Response, config::book_update_ring> book_ring{};
    lcr::lockfree::spsc_ring<schema::book::SubscribeAck, config::subscribe_ack_ring> book_subscribe_ring{};
    lcr::lockfree::spsc_ring<schema::book::UnsubscribeAck, config::unsubscribe_ack_ring> book_unsubscribe_ring{};

    // ------------------------------------------------------------
    // Construction from owning Context
    // ------------------------------------------------------------
    explicit Context(std::atomic<uint64_t>& hb_total, std::atomic<std::chrono::steady_clock::time_point>& last_hb_ts) noexcept
        : heartbeat_total(hb_total)
        , last_heartbeat_ts(last_hb_ts)
    {}
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
    std::atomic<uint64_t>& heartbeat_total;
    std::atomic<std::chrono::steady_clock::time_point>& last_heartbeat_ts;

    // Output rings for pong messages
    lcr::lockfree::spsc_ring<schema::system::Pong, config::pong_ring>& pong_ring;

    // Status
    lcr::lockfree::spsc_ring<schema::status::Update, config::status_ring>& status_ring;

    // Output rings for rejection notices
    lcr::lockfree::spsc_ring<schema::rejection::Notice, config::rejection_ring>& rejection_ring;

    // Output rings for trade channel
    lcr::lockfree::spsc_ring<schema::trade::Response, config::trade_update_ring>& trade_ring;
    lcr::lockfree::spsc_ring<schema::trade::SubscribeAck, config::subscribe_ack_ring>& trade_subscribe_ring;
    lcr::lockfree::spsc_ring<schema::trade::UnsubscribeAck, config::unsubscribe_ack_ring>& trade_unsubscribe_ring;

    // Output rings for book channel
    lcr::lockfree::spsc_ring<schema::book::Response, config::book_update_ring>& book_ring;
    lcr::lockfree::spsc_ring<schema::book::SubscribeAck, config::subscribe_ack_ring>& book_subscribe_ring;
    lcr::lockfree::spsc_ring<schema::book::UnsubscribeAck, config::unsubscribe_ack_ring>& book_unsubscribe_ring;

    // ------------------------------------------------------------
    // Construction from owning Context
    // ------------------------------------------------------------
    explicit ContextView(Context& ctx) noexcept
        : heartbeat_total(ctx.heartbeat_total)
        , last_heartbeat_ts(ctx.last_heartbeat_ts)
        , pong_ring(ctx.pong_ring)
        , status_ring(ctx.status_ring)
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

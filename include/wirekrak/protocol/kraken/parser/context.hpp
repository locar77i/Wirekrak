#pragma once

#include <atomic>
#include <chrono>

#include "wirekrak/config/ring_sizes.hpp"
#include "wirekrak/protocol/kraken/schema/rejection_notice.hpp"
#include "wirekrak/protocol/kraken/schema/system/pong.hpp"
#include "wirekrak/protocol/kraken/schema/status/update.hpp"
#include "wirekrak/protocol/kraken/schema/trade/response.hpp"
#include "wirekrak/protocol/kraken/schema/trade/response.hpp"
#include "wirekrak/protocol/kraken/schema/trade/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/schema/trade/unsubscribe_ack.hpp"
#include "wirekrak/protocol/kraken/schema/book/update.hpp"
#include "wirekrak/protocol/kraken/schema/book/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/schema/book/unsubscribe_ack.hpp"
#include "lcr/lockfree/spsc_ring.hpp"

namespace wirekrak::protocol::kraken::parser {

/*
    Context
    ---------------------------------------------------
    Shared state and output pipelines used by the Parser.
    Professional SDK pattern used by FIX, CEX/DEX MD SDKs,
    and high-performance routing systems.

    The parser writes into this context.
    The client owns the rings and wires them at construction.
*/
struct Context {
    // ------------------------------------------------------------
    // Heartbeat statistics shared with the client
    // ------------------------------------------------------------
    std::atomic<uint64_t>* heartbeat_total{nullptr};
    std::atomic<std::chrono::steady_clock::time_point>* last_heartbeat_ts{nullptr};

    // ------------------------------------------------------------
    // Output ring for pong messages
    // ------------------------------------------------------------
    lcr::lockfree::spsc_ring<kraken::system::Pong, config::pong_ring>* pong_ring{nullptr};

    // ------------------------------------------------------------
    // Output ring for rejection notices
    // ------------------------------------------------------------
    lcr::lockfree::spsc_ring<kraken::rejection::Notice, config::rejection_ring>* rejection_ring{nullptr};

    // ------------------------------------------------------------
    // Output rings for status channel
    // ------------------------------------------------------------
    lcr::lockfree::spsc_ring<kraken::status::Update, config::status_ring>* status_ring{nullptr};

    // ------------------------------------------------------------
    // Output rings for trade channel
    // ------------------------------------------------------------
    lcr::lockfree::spsc_ring<kraken::trade::Response,        config::trade_update_ring>* trade_ring{nullptr};
    lcr::lockfree::spsc_ring<kraken::trade::SubscribeAck,    config::subscribe_ack_ring>*    trade_subscribe_ring{nullptr};
    lcr::lockfree::spsc_ring<kraken::trade::UnsubscribeAck,  config::unsubscribe_ack_ring>*    trade_unsubscribe_ring{nullptr};

    // ------------------------------------------------------------
    // Output rings for book channel
    // ------------------------------------------------------------
    lcr::lockfree::spsc_ring<kraken::book::Update,       config::book_update_ring>* book_ring{nullptr};
    lcr::lockfree::spsc_ring<kraken::book::SubscribeAck,   config::subscribe_ack_ring>*  book_subscribe_ring{nullptr};
    lcr::lockfree::spsc_ring<kraken::book::UnsubscribeAck, config::unsubscribe_ack_ring>*  book_unsubscribe_ring{nullptr};
    
    // ------------------------------------------------------------
    // Convenience: check whether all pointers are valid
    // ------------------------------------------------------------
    [[nodiscard]]
    bool is_valid() const noexcept {
        return heartbeat_total &&
               last_heartbeat_ts &&
               trade_ring &&
               trade_subscribe_ring &&
               trade_unsubscribe_ring &&
               book_ring &&
               book_subscribe_ring &&
               book_unsubscribe_ring;
    }
};

} // namespace wirekrak::protocol::kraken::parser

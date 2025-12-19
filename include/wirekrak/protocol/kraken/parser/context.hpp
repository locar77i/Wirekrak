#pragma once

#include <atomic>
#include <chrono>

#include "lcr/lockfree/spsc_ring.hpp"
#include "wirekrak/protocol/kraken/system/pong.hpp"
#include "wirekrak/protocol/kraken/status/update.hpp"
#include "wirekrak/protocol/kraken/trade/response.hpp"
#include "wirekrak/protocol/kraken/trade/response.hpp"
#include "wirekrak/protocol/kraken/trade/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/trade/unsubscribe_ack.hpp"
#include "wirekrak/protocol/kraken/book/update.hpp"
#include "wirekrak/protocol/kraken/book/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/book/unsubscribe_ack.hpp"

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
    lcr::lockfree::spsc_ring<kraken::system::Pong, 8>* pong_ring{nullptr};

    // ------------------------------------------------------------
    // Output rings for status channel
    // ------------------------------------------------------------
    lcr::lockfree::spsc_ring<kraken::status::Update, 8>* status_ring{nullptr};

    // ------------------------------------------------------------
    // Output rings for trade channel
    // ------------------------------------------------------------
    lcr::lockfree::spsc_ring<kraken::trade::Response,        4096>* trade_ring{nullptr};
    lcr::lockfree::spsc_ring<kraken::trade::SubscribeAck,    8>*    trade_subscribe_ring{nullptr};
    lcr::lockfree::spsc_ring<kraken::trade::UnsubscribeAck,  8>*    trade_unsubscribe_ring{nullptr};

    // ------------------------------------------------------------
    // Output rings for book channel
    // ------------------------------------------------------------
    lcr::lockfree::spsc_ring<kraken::book::Update,       4096>* book_ring{nullptr};
    lcr::lockfree::spsc_ring<kraken::book::SubscribeAck,   8>*  book_subscribe_ring{nullptr};
    lcr::lockfree::spsc_ring<kraken::book::UnsubscribeAck, 8>*  book_unsubscribe_ring{nullptr};
    
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

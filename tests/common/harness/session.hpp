/*
===============================================================================
 Session Test Harness
===============================================================================
*/
#pragma once

#include <memory>
#include <cstdint>
#include <cassert>

#include "wirekrak/core/transport/websocket_concept.hpp"
#include "wirekrak/core/protocol/kraken/session.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/subscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/subscribe.hpp"
#include "common/mock_websocket.hpp"
#include "common/json_helpers.hpp"
#include "common/test_check.hpp"


// -----------------------------------------------------------------------------
// Setup environment
// -----------------------------------------------------------------------------
using namespace wirekrak::core;
using namespace wirekrak::core::protocol;
using namespace wirekrak::core::protocol::kraken;

using MessageRingUnderTest = lcr::lockfree::spsc_ring<transport::websocket::DataBlock, transport::RX_RING_CAPACITY>;
using WebSocketUnderTest = transport::test::MockWebSocket<MessageRingUnderTest>;

// Assert that WebSocketUnderTest conforms to transport::WebSocketConcept concept
static_assert(transport::WebSocketConcept<WebSocketUnderTest>);

static MessageRingUnderTest g_ring;   // Golbal SPSC ring buffer (transport → session)


namespace wirekrak::core::protocol::kraken::test {
namespace harness {


template<
    transport::WebSocketConcept WS,
    typename MessageRing,
//  policy::protocol::SymbolLimitConcept LimitPolicy = policy::protocol::NoSymbolLimits
    typename Bundle = policy::protocol::SessionDefault
>
struct Session {

    using SessionUnderTest = kraken::Session<WS, MessageRing, Bundle>;
    SessionUnderTest session;

    Session()
        : session(g_ring) {
        WS::reset();
    }

    // -------------------------------------------------------------------------
    // Connect
    // -------------------------------------------------------------------------
    inline void connect() {
        (void)session.connect("wss://example.com/ws");
        drain();
    }

    // -------------------------------------------------------------------------
    // Force reconnect deterministically
    // -------------------------------------------------------------------------
    inline std::uint64_t force_reconnect() {
        session.ws().emit_error(transport::Error::RemoteClosed);
        session.ws().close();
        return session.poll();
    }

    // -------------------------------------------------------------------------
    // Wait for epoch
    // -------------------------------------------------------------------------
    inline void wait_for_epoch(std::uint64_t epoch) {
        while (session.transport_epoch() < epoch) {
            (void)session.poll();
        }
    }

    // -------------------------------------------------------------------------
    // Drain session until idle
    // -------------------------------------------------------------------------
    inline void drain(int iterations = 8) {
        for (int i = 0; i < iterations; ++i) {
            (void)session.poll();
        }
    }

    // -------------------------------------------------------------------------
    // Drain rejection messages until idle
    // -------------------------------------------------------------------------
    inline void drain_rejections() {
        session.drain_rejection_messages([](const schema::rejection::Notice& msg) {
            std::cout << " -> " << msg << std::endl;
        });
    }

    // -------------------------------------------------------------------------
    // Subscribe/Unsubscribe helpers
    // -------------------------------------------------------------------------
    inline ctrl::req_id_t subscribe_trade(std::vector<std::string> symbols) {
        schema::trade::Subscribe req{.symbols = std::move(symbols)};
        return session.subscribe(req);
    }

    inline ctrl::req_id_t subscribe_trade(std::initializer_list<std::string> symbols) {
        return subscribe_trade(std::vector<std::string>{symbols});
    }

    inline ctrl::req_id_t subscribe_trade(const std::string& symbol) {
        return subscribe_trade(std::vector<std::string>{symbol});
    }

    inline ctrl::req_id_t unsubscribe_trade(const std::string& symbol) {
        schema::trade::Unsubscribe unsub{.symbols = {symbol}};
        return session.unsubscribe(unsub);
    }

    inline ctrl::req_id_t subscribe_book(std::vector<std::string> symbols, int depth) {
        schema::book::Subscribe sub{.symbols = std::move(symbols), .depth = depth};
        return session.subscribe(sub);
    }

    inline ctrl::req_id_t subscribe_book(std::initializer_list<std::string> symbols, int depth) {
        return subscribe_book(std::vector<std::string>{symbols}, depth);
    }

    inline ctrl::req_id_t subscribe_book(const std::string& symbol, int depth) {
        return subscribe_book(std::vector<std::string>{std::move(symbol)}, depth);
    }

    inline ctrl::req_id_t unsubscribe_book(const std::string& symbol, int depth) {
        schema::book::Unsubscribe unsub{.symbols = {symbol}, .depth = depth};
        return session.unsubscribe(unsub);
    }

    // -------------------------------------------------------------------------
    // Subscribe/Unsubscribe ACK helpers
    // -------------------------------------------------------------------------
    inline void confirm_trade_subscription(ctrl::req_id_t req_id, const std::string& sym) {
        assert(req_id != ctrl::INVALID_REQ_ID && "Request ID cannot be invalid");
        assert(req_id >= 10 && "For trade subscriptions req_id should be >= 10");
        session.ws().emit_message(json::ack::trade_sub(req_id, sym));
        (void)session.poll();
    }

    inline void confirm_trade_unsubscription(ctrl::req_id_t req_id, const std::string& sym) {
        assert(req_id != ctrl::INVALID_REQ_ID && "Request ID cannot be invalid");
        assert(req_id >= 10 && "For trade unsubscriptions req_id should be >= 10");
        session.ws().emit_message(json::ack::trade_unsub(req_id, sym));
        (void)session.poll();
    }

    inline void confirm_book_subscription(ctrl::req_id_t req_id, const std::string& sym, int depth) {
        assert(req_id != ctrl::INVALID_REQ_ID && "Request ID cannot be invalid");
        assert(req_id >= 10 && "For book subscriptions req_id should be >= 10");
        session.ws().emit_message(json::ack::book_sub(req_id, sym, depth));
        (void)session.poll();
    }

    inline void confirm_book_unsubscription(ctrl::req_id_t req_id, const std::string& sym, int depth) {
        assert(req_id != ctrl::INVALID_REQ_ID && "Request ID cannot be invalid");
        assert(req_id >= 10 && "For book unsubscriptions req_id should be >= 10");
        session.ws().emit_message(json::ack::book_unsub(req_id, sym, depth));
        (void)session.poll();
    }

    // -------------------------------------------------------------------------
    // Rejection helpers
    // -------------------------------------------------------------------------

    inline void reject(std::string_view method, ctrl::req_id_t req_id, const std::string& sym, std::string_view error) {
        assert(req_id != ctrl::INVALID_REQ_ID && "Request ID cannot be invalid");
        assert(req_id >= 10 && "For rejection notices, req_id should be >= 10");
        session.ws().emit_message(json::ack::rejection_notice(method, req_id, sym, std::string(error)));
        (void)session.poll();
    }

    inline void reject_trade_subscription(ctrl::req_id_t req_id, const std::string& sym) {
        reject("subscribe", req_id, sym, "Subscription rejected");
    }

    inline void reject_trade_unsubscription(ctrl::req_id_t req_id, const std::string& sym) {
        reject("unsubscribe", req_id, sym, "Unsubscription rejected");
    }

    inline void reject_book_subscription(ctrl::req_id_t req_id, const std::string& sym) {
        reject("subscribe", req_id, sym, "Subscription rejected");
    }

    inline void reject_book_unsubscription(ctrl::req_id_t req_id, const std::string& sym) {
        reject("unsubscribe", req_id, sym, "Unsubscription rejected");
    }
};

} // namespace harness

using SessionHarness = harness::Session<WebSocketUnderTest, MessageRingUnderTest>;

} // namespace wirekrak::core::protocol::kraken::test

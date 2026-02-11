#pragma once

#include <memory>
#include <cstdint>

#include "wirekrak/core/protocol/kraken/session.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/subscribe.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/subscribe.hpp"
#include "common/mock_websocket.hpp"
#include "common/json_helpers.hpp"
#include "common/test_check.hpp"

namespace ctrl = wirekrak::core::protocol::ctrl;


namespace wirekrak::core::protocol::kraken::test::harness {

struct Session {

    kraken::Session<transport::test::MockWebSocket> session;

    Session() {
        wirekrak::core::transport::test::MockWebSocket::reset();
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
    inline void force_reconnect() {
        session.ws().emit_error(transport::Error::RemoteClosed);
        session.ws().close();
        (void)session.poll();
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
    // Subscribe/Unsubscribe helpers
    // -------------------------------------------------------------------------
    ctrl::req_id_t subscribe_trade(const std::string& symbol) {
        schema::trade::Subscribe sub{.symbols = {symbol}};
        return session.subscribe(sub);
    }

    ctrl::req_id_t unsubscribe_trade(const std::string& symbol) {
        schema::trade::Unsubscribe unsub{.symbols = {symbol}};
        return session.unsubscribe(unsub);
    }

    ctrl::req_id_t subscribe_book(const std::string& symbol, int depth) {
        schema::book::Subscribe sub{.symbols = {symbol}, .depth = depth};
        return session.subscribe(sub);
    }

    ctrl::req_id_t unsubscribe_book(const std::string& symbol, int depth) {
        schema::book::Unsubscribe unsub{.symbols = {symbol}, .depth = depth};
        return session.unsubscribe(unsub);
    }

    // -------------------------------------------------------------------------
    // Subscribe/Unsubscribe ACK helpers
    // -------------------------------------------------------------------------
    inline void confirm_trade_subscription(ctrl::req_id_t id, const std::string& sym) {
        session.ws().emit_message(json::ack::trade_sub(id, sym));
        (void)session.poll();
    }

    inline void confirm_trade_unsubscription(ctrl::req_id_t id, const std::string& sym) {
        session.ws().emit_message(json::ack::trade_unsub(id, sym));
        (void)session.poll();
    }

    inline void confirm_book_subscription(ctrl::req_id_t id, const std::string& sym, int depth) {
        session.ws().emit_message(json::ack::book_sub(id, sym, depth));
        (void)session.poll();
    }

    inline void confirm_book_unsubscription(ctrl::req_id_t id, const std::string& sym, int depth) {
        session.ws().emit_message(json::ack::book_unsub(id, sym, depth));
        (void)session.poll();
    }

    // -------------------------------------------------------------------------
    // Rejection helpers
    // -------------------------------------------------------------------------

    inline void reject(std::string_view method, ctrl::req_id_t id, const std::string& sym, std::string_view error) {
        session.ws().emit_message(json::ack::rejection_notice(method, id, sym, std::string(error)));
        (void)session.poll();
    }

    inline void reject_trade_subscription(ctrl::req_id_t id, const std::string& sym) {
        reject("subscribe", id, sym, "Subscription rejected");
    }

    inline void reject_trade_unsubscription(ctrl::req_id_t id, const std::string& sym) {
        reject("unsubscribe", id, sym, "Unsubscription rejected");
    }

    inline void reject_book_subscription(ctrl::req_id_t id, const std::string& sym) {
        reject("subscribe", id, sym, "Subscription rejected");
    }

    inline void reject_book_unsubscription(ctrl::req_id_t id, const std::string& sym) {
        reject("unsubscribe", id, sym, "Unsubscription rejected");
    }
};

} // namespace wirekrak::core::protocol::kraken::test::harness


using SessionHarness = wirekrak::core::protocol::kraken::test::harness::Session;

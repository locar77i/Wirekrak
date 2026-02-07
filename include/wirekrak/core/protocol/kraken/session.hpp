#pragma once


#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <chrono>
#include <utility>

#include "wirekrak/core/config/ring_sizes.hpp"
#include "wirekrak/core/transport/connection.hpp"
#include "wirekrak/core/transport/winhttp/websocket.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"
#include "wirekrak/core/protocol/policy/liveness.hpp"
#include "wirekrak/core/protocol/kraken/context.hpp"
#include "wirekrak/core/protocol/kraken/request/concepts.hpp"
#include "wirekrak/core/protocol/kraken/schema/system/ping.hpp"
#include "wirekrak/core/protocol/kraken/channel_traits.hpp"
#include "wirekrak/core/protocol/kraken/request/concepts.hpp"
#include "wirekrak/core/protocol/kraken/parser/router.hpp"
#include "wirekrak/core/protocol/kraken/dispatcher.hpp"
#include "wirekrak/core/protocol/kraken/channel/manager.hpp"
#include "wirekrak/core/protocol/kraken/replay/database.hpp"
#include "wirekrak/core/protocol/kraken/response/partitioner.hpp"
#include "lcr/log/logger.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/sequence.hpp"


namespace wirekrak::core {
namespace protocol {
namespace kraken {

/*
===============================================================================
Kraken protocol Session
===============================================================================

This session implements the Kraken WebSocket API on top of Wirekrak’s generic
streaming infrastructure.

Design principles:
  - Composition over inheritance
  - Clear separation between transport, streaming, and protocol logic
  - Zero runtime polymorphism
  - Compile-time safety via C++20 concepts
  - Low-latency, event-driven design

Architecture:
  - transport::*        → WebSocket transport (WinHTTP, mockable)
  - transport::Connection      → Generic streaming client
                           • connection lifecycle
                           • reconnection
                           • heartbeat & liveness
                           • raw message delivery
  - protocol::kraken    → Protocol-specific logic
                           • request serialization
                           • message routing
                           • schema validation
                           • domain models

The Kraken session:
  - Owns a transport::Connection instance via composition
  - Registers internal handlers to translate raw messages into typed events
  - Exposes a *protocol-oriented API* (subscribe, unsubscribe, ping, etc.)
  - Intentionally does NOT expose low-level transport hooks directly

Rationale:
  - End users interact with Kraken concepts, not transport mechanics
  - Streaming concerns (reconnect, liveness) are centralized and reusable
  - Prevents misuse and enforces correct protocol behavior
  - Keeps the public API minimal, explicit, and stable

Advanced users may still customize behavior by:
  - Providing alternative transports
  - Extending protocol routing internally
  - Observing higher-level protocol events

===============================================================================
*/


template<transport::WebSocketConcept WS>
class Session {
    using pong_handler_t       = std::function<void(const schema::system::Pong&)>;
    using rejection_handler_t  = std::function<void(const schema::rejection::Notice&)>;
    using status_handler_t     = std::function<void(const schema::status::Update&)>;

public:
    Session()
        : ctx_{connection_.heartbeat_total(), connection_.last_heartbeat_ts()}
        , ctx_view_{ctx_}
        , parser_(ctx_view_)
    {
        connection_.on_message([this](std::string_view msg) {
            handle_message_(msg);
        });
    }

    // open connection
    [[nodiscard]]
    inline bool connect(const std::string& url) {
        return connection_.open(url) == transport::Error::None;
    }

    // close connection
    inline void close() {
        connection_.close();
    }

    // Register pong callback
    inline void on_pong(pong_handler_t cb) noexcept {
        hooks_.handle_pong = std::move(cb);
    }

    // Register rejection callback
    inline void on_rejection(rejection_handler_t cb) noexcept {
        hooks_.handle_rejection = std::move(cb);
    }

    // Register status callback
    inline void on_status(status_handler_t cb) noexcept {
        hooks_.handle_status = std::move(cb);
    }

    // Send ping
    inline void ping() noexcept{
        send_raw_request_(schema::system::Ping{.req_id = control::PING_ID});
    }

    template <request::Subscription RequestT, class Callback>
    inline void subscribe(const RequestT& req, Callback&& cb) {
        static_assert(request::ValidRequestIntent<RequestT>,
            "Invalid request type: a request must define exactly one intent tag (subscribe_tag, unsubscribe_tag, control_tag...)"
        );
        using ResponseT = typename channel_traits<RequestT>::response_type;
        static_assert(requires { req.symbols; }, "Request must expose a member called `symbols`");
        // 1) Store callback safely once
        using StoredCallback = std::function<void(const ResponseT&)>;
        StoredCallback cb_copy = std::forward<Callback>(cb);
        // Register callback for the symbol(s)
        for (const auto& symbol : req.symbols) {
            dispatcher_.add_handler<ResponseT>(symbol, cb_copy);
        }
        // TODO: Handle duplicate symbol subscriptions to avoid kraken rejections
        subscribe_with_ack_(req, cb_copy);
    }

    template <request::Unsubscription RequestT>
    inline void unsubscribe(const RequestT& req) {
        static_assert(request::ValidRequestIntent<RequestT>,
            "Invalid request type: a request must define exactly one intent tag (subscribe_tag, unsubscribe_tag, control_tag...)"
        );
        unsubscribe_with_ack_(req);
    }

    // -----------------------------------------------------------------------------
    // NOTE (Hackathon scope)
    // -----------------------------------------------------------------------------
    // The current poll() implementation explicitly drains each ring buffer in-line.
    // While functionally correct, this results in repetitive boilerplate and mixes
    // event-loop orchestration with message-specific handling.
    //
    // A future refactor should introduce a generic ring-draining helper and/or
    // group ring processing by domain (system, trade, book, etc.) to improve
    // readability, extensibility, and maintainability.
    //
    // This refactor was intentionally deferred to prioritize correctness,
    // stability, and timely hackathon submission.
    // -----------------------------------------------------------------------

    // TODO: Refactor poll method for incoming messages and events
    [[nodiscard]]
    inline uint64_t poll() {
        // === Heartbeat liveness & reconnection logic ===
        connection_.poll();
        transport::connection::Signal sig;
        while (connection_.poll_signal(sig)) {
            handle_connection_signal_(sig);
        }

        // ===============================================================================
        // PROCESS PONG MESSAGES
        // ===============================================================================
        { // === Process pong ring ===
        schema::system::Pong pong;
        while (ctx_.pong_ring.pop(pong)) {
            handle_pong_(pong);
        }}
        // ===============================================================================
        // PROCESS REJECTION NOTICES
        // ===============================================================================
        { // === Process rejection ring ===
        schema::rejection::Notice notice;
        while (ctx_.rejection_ring.pop(notice)) {
            handle_rejection_(notice);
        }}
        // ===============================================================================
        // PROCESS STATUS MESSAGES
        // ===============================================================================
        { // === Process status ring ===
        schema::status::Update update;
        while (ctx_.status_ring.pop(update)) {
            handle_status_(update);
        }}
        // ===============================================================================
        // PROCESS TRADE MESSAGES
        // ===============================================================================
        { // === Process trade ring ===
        schema::trade::Response resp;
        while (ctx_.trade_ring.pop(resp)) {
            trade_partitioner.reset(resp);
            for (const auto& view : trade_partitioner.views()) {
                dispatcher_.dispatch(view);
            }
        }}
        { // === Process trade subscribe ring ===
        schema::trade::SubscribeAck ack;
        while (ctx_.trade_subscribe_ring.pop(ack)) {
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SUBMGR] Subscription ACK missing req_id for channel 'trade' {" << ack.symbol << "}");
            }
            else {
                trade_channel_manager_.process_subscribe_ack(Channel::Trade, ack.req_id.value(), ack.symbol, ack.success);
            }
        }}
        { // === Process trade unsubscribe ring ===
        schema::trade::UnsubscribeAck ack;
        while (ctx_.trade_unsubscribe_ring.pop(ack)) {
            dispatcher_.remove_symbol_handlers<schema::trade::UnsubscribeAck>(ack.symbol);
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SUBMGR] Unsubscription ACK missing req_id for channel 'trade' {" << ack.symbol << "}");
            }
            else {
                trade_channel_manager_.process_unsubscribe_ack(Channel::Trade, ack.req_id.value(), ack.symbol, ack.success);
            }
        }}
        // ===============================================================================
        // PROCESS BOOK UPDATES
        // ===============================================================================
        { // === Process book ring ===
        schema::book::Response resp;
        while (ctx_.book_ring.pop(resp)) {
            dispatcher_.dispatch(resp);
        }}
        { // === Process book subscribe ring ===
        schema::book::SubscribeAck ack;
        while (ctx_.book_subscribe_ring.pop(ack)) {
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SUBMGR] Subscription ACK missing req_id for channel 'book' {" << ack.symbol << "}");
            }
            else {
                book_channel_manager_.process_subscribe_ack(Channel::Book, ack.req_id.value(), ack.symbol, ack.success);
            }
        }}
        { // === Process book unsubscribe ring ===
        schema::book::UnsubscribeAck ack;
        while (ctx_.book_unsubscribe_ring.pop(ack)) {
            dispatcher_.remove_symbol_handlers<schema::book::UnsubscribeAck>(ack.symbol);
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SUBMGR] Unsubscription ACK missing req_id for channel 'book' {" << ack.symbol << "}");
            }
            else {
                book_channel_manager_.process_unsubscribe_ack(Channel::Book, ack.req_id.value(), ack.symbol, ack.success);
            }
        }}

        return connection_.epoch();
    }

    // Set liveness policy
    inline void set_policy(policy::Liveness p) noexcept {
        liveness_policy_ = p;
    }

    // Accessor to the heartbeat counter
    [[nodiscard]]
    inline uint64_t heartbeat_total() const noexcept {
        return connection_.heartbeat_total().load(std::memory_order_relaxed);
    }

    // Accessor to the trade subscription manager
    [[nodiscard]]
    inline const channel::Manager& trade_subscriptions() const noexcept {
        return trade_channel_manager_;
    }

    // Accessor to the book subscription manager
    [[nodiscard]]
    inline const channel::Manager& book_subscriptions() const noexcept {
        return book_channel_manager_;
    }

    // -----------------------------------------------------------------------------
    // Transport progress facts
    // -----------------------------------------------------------------------------

    // Accessor to the current transport epoch
    // incremented on each successful connect, used for staleness checks
    [[nodiscard]]
    inline uint64_t transport_epoch() const noexcept {
        return connection_.epoch();
    }

    [[nodiscard]]
    inline uint64_t rx_messages() const noexcept {
        return connection_.rx_messages();
    }

    [[nodiscard]]
    inline uint64_t tx_messages() const noexcept {
        return connection_.tx_messages();
    }

    [[nodiscard]]
    inline uint64_t hb_messages() const noexcept {
        return connection_.hb_messages();
    }

private:
    // Sequence generator for request IDs
    lcr::sequence req_id_seq_{control::PROTOCOL_BASE};

    // Underlying streaming client (and telemetry)
    transport::telemetry::Connection telemetry_;
    transport::Connection<WS> connection_{telemetry_};

    // Hooks structure to store all user-defined callbacks
    struct Hooks {
        pong_handler_t             handle_pong{};             // Pong callback
        rejection_handler_t        handle_rejection{};        // Rejection callback
        status_handler_t           handle_status{};           // Status callback
    };

    // Handlers bundle
    Hooks hooks_;

    // Liveness policy
    policy::Liveness liveness_policy_{policy::Liveness::Passive};

    // Session context (owning)
    Context ctx_;

    // Session context view (non-owning)
    ContextView ctx_view_;

    // Protocol parser / router
    parser::Router parser_;

    // Message dispatcher
    Dispatcher dispatcher_;

    // Channel subscription managers
    channel::Manager trade_channel_manager_;
    channel::Manager book_channel_manager_;

    // Response classifiers (reused, allocation-stable)
    response::Partitioner<schema::trade::Response> trade_partitioner;

    // Replay database
    replay::Database replay_db_;

private:
    inline void handle_connect_() {
        WK_TRACE("[SESSION] handle connect (transport_epoch = " << transport_epoch() << ")");
        // Replay subscriptions if this is a reconnect (epoch > 1)
        if (transport_epoch() > 1) {
            // 1) Replay trade subscriptions
            auto trade_subscriptions = replay_db_.take_subscriptions<schema::trade::Subscribe>();
            if(!trade_subscriptions.empty()) {
                WK_DEBUG("[REPLAY] Replaying " << trade_subscriptions.size() << " trade subscription(s)");
                for (const auto& subscription : trade_subscriptions) {
                    subscribe(subscription.request(), subscription.callback());
                }
            }
            else {
                WK_DEBUG("[REPLAY] No trade subscriptions to replay");
            }
            // 2) Replay book subscriptions
            auto book_subscriptions = replay_db_.take_subscriptions<schema::book::Subscribe>();
            if(!book_subscriptions.empty()) {
                WK_DEBUG("[REPLAY] Replaying " << book_subscriptions.size() << " book subscription(s)");
                for (const auto& subscription : book_subscriptions) {
                    subscribe(subscription.request(), subscription.callback());
                }
            }
            else {
                WK_DEBUG("[REPLAY] No book subscriptions to replay");
            }
        }
    }

    inline void handle_disconnect_() {
        WK_TRACE("[SESSION] handle disconnect (transport_epoch = " << transport_epoch() << ")");
        // Clear runtime state
        dispatcher_.clear();
        trade_channel_manager_.clear_all();
        book_channel_manager_.clear_all();
    }

    inline void handle_message_(std::string_view sv) {
        parser_.parse_and_route(sv);
    }

    inline void handle_pong_(const schema::system::Pong& pong) noexcept {
        if (hooks_.handle_pong) {
            hooks_.handle_pong(pong);
        }
    }

    inline void handle_rejection_(const schema::rejection::Notice& notice) noexcept {
        if (hooks_.handle_rejection) {
            if (notice.req_id.has()) {
                if (notice.symbol.has()) {
                    // we cannot infer channel from notice, so we try all managers
                    trade_channel_manager_.try_process_rejection(notice.req_id.value(), notice.symbol.value());
                    book_channel_manager_.try_process_rejection(notice.req_id.value(), notice.symbol.value());
                    replay_db_.try_process_rejection(notice.req_id.value(), notice.symbol.value());
                }
            }
            
            hooks_.handle_rejection(notice);
        }
    }
    
    inline void handle_status_(const schema::status::Update& status) noexcept {
        if (hooks_.handle_status) {
            hooks_.handle_status(status);
        }
    }

    inline void handle_connection_signal_(transport::connection::Signal sig) noexcept {
        switch (sig) {
        case transport::connection::Signal::Connected:
            handle_connect_();
            break;
        case transport::connection::Signal::Disconnected:
            handle_disconnect_();
            break;
        case transport::connection::Signal::RetryScheduled:
            // Currently no user-defined hook for retry scheduled
            break;
        case transport::connection::Signal::LivenessThreatened:
            // Currently no user-defined hook for liveness warning
            if (liveness_policy_ == policy::Liveness::Active) {
                    send_raw_request_(schema::system::Ping{.req_id = control::PING_ID});
                }
            break;
        default:
            break;
        }
    }

    // Helpers to get the correct subscription manager
    template<class MessageT>
    auto& subscription_manager_for_() {
        if constexpr (channel_of_v<MessageT> == Channel::Trade) {
            return trade_channel_manager_;
        }
        else if constexpr (channel_of_v<MessageT> == Channel::Book) {
            return book_channel_manager_;
        }
        // else if constexpr (...) return ticker_handlers_;
    }

    template<class MessageT>
    const auto& subscription_manager_for_() const {
        if constexpr (channel_of_v<MessageT> == Channel::Trade) {
            return trade_channel_manager_;
        }
        else if constexpr (channel_of_v<MessageT> == Channel::Book) {
            return book_channel_manager_;
        }
        // else if constexpr (...) return ticker_handlers_;
    }    

    // Send raw request (used for control messages)
    template <request::Control RequestT>
    inline void send_raw_request_(RequestT req) {
        static_assert(request::ValidRequestIntent<RequestT>,
            "Invalid request type: a request must define exactly one intent tag (subscribe_tag, unsubscribe_tag, control_tag...)"
        );
        // 1) Assign req_id if missing
        if (!req.req_id.has()) {
            req.req_id = req_id_seq_.next();
        }
        std::string json = req.to_json();
        if (!connection_.send(json)) {
            WK_ERROR("Failed to send raw message: " << json);
        }
    }

    // Perform subscription with ACK handling
    template<class RequestT, class Callback>
    inline void subscribe_with_ack_(RequestT req, Callback&& cb) {
        WK_INFO("Subscribing to channel '" << channel_name_of_v<RequestT> << "' " << core::to_string(req.symbols));
        using ResponseT = typename channel_traits<RequestT>::response_type;
        using StoredCallback = std::function<void(const ResponseT&)>;
        // 1) Assign req_id if missing
        if (!req.req_id.has()) {
            req.req_id = req_id_seq_.next();
        }
        // 2) Store callback safely once and register in replay DB
        StoredCallback cb_copy = std::forward<Callback>(cb);
        replay_db_.add(req, cb_copy);
        // 3) Send JSON BEFORE moving req.symbols
        WK_DEBUG("Sending subscribe message: " << req.to_json());
        if (!connection_.send(req.to_json())) {
            WK_ERROR("Failed to send subscription request for req_id=" << lcr::to_string(req.req_id));
            return;
        }
        // 4) Tell subscription manager we are awaiting an ACK (transfer ownership of symbols)
        subscription_manager_for_<RequestT>().register_subscription(
            std::move(req.symbols),
            req.req_id.value()
        );
    }

    // Perform unsubscription with ACK handling
    template<class RequestT>
    inline void unsubscribe_with_ack_(RequestT req) {
        WK_INFO("Unsubscribing from channel '" << channel_name_of_v<RequestT> << "' " << core::to_string(req.symbols));
        // 1) Assign req_id if missing
        if (!req.req_id.has()) {
            req.req_id = req_id_seq_.next();
        }
        // 2) Register in replay DB (no callback needed for unsubscription)
        replay_db_.remove(req);
        // 3) Send JSON BEFORE moving req.symbols
        WK_DEBUG("Sending unsubscribe message: " << req.to_json());
        if (!connection_.send(req.to_json())) {
            WK_ERROR("Failed to send unsubscription request for req_id=" << lcr::to_string(req.req_id));
            return;
        }
        // 4) Tell subscription manager we are awaiting an ACK (transfer ownership of symbols)
        subscription_manager_for_<RequestT>().register_unsubscription(
            std::move(req.symbols),
            req.req_id.value()
        );
    }
};


} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core

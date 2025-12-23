#pragma once


#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <chrono>
#include <utility>

#include "wirekrak/config/ring_sizes.hpp"
#include "wirekrak/stream/client.hpp"
#include "wirekrak/transport/winhttp/websocket.hpp"
#include "wirekrak/protocol/kraken/context.hpp"
#include "wirekrak/protocol/kraken/request/concepts.hpp"
#include "wirekrak/protocol/kraken/schema/system/ping.hpp"
#include "wirekrak/protocol/kraken/channel_traits.hpp"
#include "wirekrak/protocol/kraken/request/concepts.hpp"
#include "wirekrak/protocol/kraken/parser/router.hpp"
#include "wirekrak/protocol/kraken/dispatcher.hpp"
#include "wirekrak/protocol/kraken/channel/manager.hpp"
#include "wirekrak/protocol/kraken/replay/database.hpp"
#include "lcr/log/logger.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/sequence.hpp"


namespace wirekrak {
namespace protocol {
namespace kraken {

/*
===============================================================================
Kraken Streaming Client
===============================================================================

This client implements the Kraken WebSocket API on top of Wirekrak’s generic
streaming infrastructure.

Design principles:
  - Composition over inheritance
  - Clear separation between transport, streaming, and protocol logic
  - Zero runtime polymorphism
  - Compile-time safety via C++20 concepts
  - Low-latency, event-driven design

Architecture:
  - transport::*        → WebSocket transport (WinHTTP, mockable)
  - stream::Client      → Generic streaming client
                           • connection lifecycle
                           • reconnection
                           • heartbeat & liveness
                           • raw message delivery
  - protocol::kraken    → Protocol-specific logic
                           • request serialization
                           • message routing
                           • schema validation
                           • domain models

The Kraken client:
  - Owns a stream::Client instance via composition
  - Registers internal handlers to translate raw messages into typed events
  - Exposes a *protocol-oriented API* (subscribe, unsubscribe, ping, etc.)
  - Intentionally does NOT expose low-level stream hooks directly

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
class Client {
    using pong_handler_t = std::function<void(const system::Pong&)>;
    using rejection_handler_t = std::function<void(const rejection::Notice&)>;
    using status_handler_t = std::function<void(const status::Update&)>;

public:
    Client()
        : ctx_{stream_.heartbeat_total(), stream_.last_heartbeat_ts()}
        , ctx_view_{ctx_}
        , parser_(ctx_view_)
    {
        stream_.on_connect([this]() {
            handle_connect_();
        });

        stream_.on_disconnect([this]() {
            handle_disconnect_();
        });

        stream_.on_message([this](std::string_view msg) {
            handle_message_(msg);
        });

        stream_.on_liveness_timeout([this]() {
            handle_liveness_timeout_();
        });
    }

    [[nodiscard]]
    inline bool connect(const std::string& url) {
        return stream_.connect(url);
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
    inline void ping(lcr::optional<std::uint64_t> req_id = {}) noexcept{
        system::Ping ping{.req_id = req_id};
        send_raw_request_(system::Ping{.req_id = req_id});
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

    // Poll for incoming messages and events
    inline void poll() {
        // === Heartbeat liveness & reconnection logic ===
        stream_.poll();
        // ===============================================================================
        // PROCESS PONG MESSAGES
        // ===============================================================================
        { // === Process pong ring ===
        system::Pong pong;
        while (ctx_.pong_ring.pop(pong)) {
            handle_pong_(pong);
        }}
        // ===============================================================================
        // PROCESS REJECTION NOTICES
        // ===============================================================================
        { // === Process rejection ring ===
        rejection::Notice notice;
        while (ctx_.rejection_ring.pop(notice)) {
            handle_rejection_(notice);
        }}
        // ===============================================================================
        // PROCESS STATUS MESSAGES
        // ===============================================================================
        { // === Process status ring ===
        status::Update update;
        while (ctx_.status_ring.pop(update)) {
            handle_status_(update);
        }}
        // ===============================================================================
        // PROCESS TRADE MESSAGES
        // ===============================================================================
        { // === Process trade ring ===
        trade::Response resp;
        while (ctx_.trade_ring.pop(resp)) {
            for (auto& trade_msg : resp.trades) {
                dispatcher_.dispatch(trade_msg);
            }
        }}
        { // === Process trade subscribe ring ===
        trade::SubscribeAck ack;
        while (ctx_.trade_subscribe_ring.pop(ack)) {
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SUBMGR] Subscription ACK missing req_id for channel 'trade' {" << ack.symbol << "}");
                return;
            }
            trade_channel_manager_.process_subscribe_ack(Channel::Trade, ack.req_id.value(), ack.symbol, ack.success);
        }}
        { // === Process trade unsubscribe ring ===
        trade::UnsubscribeAck ack;
        while (ctx_.trade_unsubscribe_ring.pop(ack)) {
            dispatcher_.remove_symbol_handlers<trade::UnsubscribeAck>(ack.symbol);
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SUBMGR] Unsubscription ACK missing req_id for channel 'trade' {" << ack.symbol << "}");
                return;
            }
            trade_channel_manager_.process_unsubscribe_ack(Channel::Trade, ack.req_id.value(), ack.symbol, ack.success);
        }}
        // ===============================================================================
        // PROCESS BOOK UPDATES
        // ===============================================================================
        { // === Process book ring ===
        book::Response resp;
        while (ctx_.book_ring.pop(resp)) {
            dispatcher_.dispatch(resp);
        }}
        { // === Process book subscribe ring ===
        book::SubscribeAck ack;
        while (ctx_.book_subscribe_ring.pop(ack)) {
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SUBMGR] Subscription ACK missing req_id for channel 'book' {" << ack.symbol << "}");
                return;
            }
            book_channel_manager_.process_subscribe_ack(Channel::Book, ack.req_id.value(), ack.symbol, ack.success);
        }}
        { // === Process book unsubscribe ring ===
        book::UnsubscribeAck ack;
        while (ctx_.book_unsubscribe_ring.pop(ack)) {
            dispatcher_.remove_symbol_handlers<book::UnsubscribeAck>(ack.symbol);
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SUBMGR] Unsubscription ACK missing req_id for channel 'book' {" << ack.symbol << "}");
                return;
            }
            book_channel_manager_.process_unsubscribe_ack(Channel::Book, ack.req_id.value(), ack.symbol, ack.success);
        }}
    }

    // Accessor to the heartbeat counter
    [[nodiscard]]
    inline uint64_t heartbeat_total() const noexcept {
        return stream_.heartbeat_total().load(std::memory_order_relaxed);
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


private:
    // Sequence generator for request IDs
    lcr::sequence req_id_seq_{};

    // Underlying streaming client (composition)
    wirekrak::stream::Client<WS> stream_;

    // Hooks structure to store all user-defined callbacks
    struct Hooks {
        pong_handler_t handle_pong{};            // Pong callback
        rejection_handler_t handle_rejection{};  // Rejection callback
        status_handler_t handle_status{};        // Status callback
    };

    // Handlers bundle
    Hooks hooks_;

    // Client context (owning)
    Context ctx_;

    // Client context view (non-owning)
    ContextView ctx_view_;

    // Protocol parser / router
    parser::Router parser_;

    // Message dispatcher
    Dispatcher dispatcher_;

    // Channel subscription managers
    channel::Manager trade_channel_manager_;
    channel::Manager book_channel_manager_;

    // Replay database
    replay::Database replay_db_;

private:
    inline void handle_connect_() {
        // 1) Clear runtime state
        dispatcher_.clear();
        trade_channel_manager_.clear_all();
        book_channel_manager_.clear_all();
        // 2) Replay trade subscriptions
        auto trade_subscriptions = replay_db_.take_subscriptions<trade::Subscribe>();
        for (const auto& subscription : trade_subscriptions) {
            subscribe(subscription.request(), subscription.callback());
        }
        // 3) Replay book subscriptions
        auto book_subscriptions = replay_db_.take_subscriptions<book::Subscribe>();
        for (const auto& subscription : book_subscriptions) {
            subscribe(subscription.request(), subscription.callback());
        }
    }

    inline void handle_disconnect_() {
        // handle disconnect
    }

    inline void handle_message_(std::string_view sv) {
        parser_.parse_and_route(sv);
    }

    inline void handle_liveness_timeout_() {
        // handle liveness timeout
    }

    inline void handle_pong_(const system::Pong& pong) noexcept {
        if (hooks_.handle_pong) {
            hooks_.handle_pong(pong);
        }
    }

    inline void handle_rejection_(const rejection::Notice& notice) noexcept {
        if (hooks_.handle_rejection) {
            WK_WARN("[!!] TODO: Rejection internal management (f. ex. drop invalid symbols, ...)");
            hooks_.handle_rejection(notice);
        }
    }
    
    inline void handle_status_(const status::Update& status) noexcept {
        if (hooks_.handle_status) {
            hooks_.handle_status(status);
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
        if (!stream_.send(json)) {
            WK_ERROR("Failed to send raw message: " << json);
        }
    }

    // Perform subscription with ACK handling
    template<class RequestT, class Callback>
    inline void subscribe_with_ack_(RequestT req, Callback&& cb) {
        WK_DEBUG("subscribe_with_ack_() called: " << req.to_json());
        using ResponseT = typename channel_traits<RequestT>::response_type;
        using StoredCallback = std::function<void(const ResponseT&)>;
        // 1) Assign req_id if missing
        if (!req.req_id.has()) {
            req.req_id = req_id_seq_.next();
        }
        WK_INFO("Subscribing to channel '" << channel_name_of_v<RequestT> << "' " << wirekrak::to_string(req.symbols) << " with req_id=" << lcr::to_string(req.req_id));
        // 2) Store callback safely once and register in replay DB
        StoredCallback cb_copy = std::forward<Callback>(cb);
        replay_db_.add(req, cb_copy);
        // 3) Send JSON BEFORE moving req.symbols
        if (!stream_.send(req.to_json())) {
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
        WK_DEBUG("unsubscribe_with_ack_() called: " << req.to_json());
        // 1) Assign req_id if missing
        if (!req.req_id.has()) {
            req.req_id = req_id_seq_.next();
        }
        WK_INFO("Unsubscribing from channel '" << channel_name_of_v<RequestT> << "' " << wirekrak::to_string(req.symbols) << " with req_id=" << lcr::to_string(req.req_id));
        // 2) Register in replay DB (no callback needed for unsubscription)
        replay_db_.remove(req);
        // 3) Send JSON BEFORE moving req.symbols
        if (!stream_.send(req.to_json())) {
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
} // namespace wirekrak

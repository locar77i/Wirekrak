#pragma once

#include <string>
#include <thread>

#include "wirekrak/winhttp/websocket.hpp"
#include "wirekrak/parser.hpp"
#include "wirekrak/dispatcher.hpp"
#include "wirekrak/channel/manager.hpp"
#include "wirekrak/schema/trade/Subscribe.hpp"
#include "wirekrak/schema/trade/Unsubscribe.hpp"
#include "wirekrak/schema/trade/Response.hpp"
#include "wirekrak/replay/database.hpp"
#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/channel_traits.hpp"
#include "wirekrak/core/transport/websocket.hpp"
#include "lcr/log/logger.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/sequence.hpp"


namespace wirekrak {

template<transport::WebSocket WS>
class Client {
public:
    Client()
        : parser_(heartbeat_total_, trade_ring_, trade_subscribe_ring_, trade_unsubscribe_ring_) {
        ws_.set_message_callback([this](const std::string& msg){
            parser_.parse_and_route(msg);
        });
    }

    ~Client() {
        ws_.close();
    }

    [[nodiscard]] inline bool connect(const std::string& url) {
        last_url_ = url;
        ParsedUrl parsed_url;
        try {
            parsed_url = parse_url_(url);
        }
        catch (const std::exception& e) {
            WK_ERROR("URL parse error: " << e.what());
            return false;
        }
        WK_INFO("Connecting to: " << parsed_url.scheme << "://" << parsed_url.host << ":" << parsed_url.port << parsed_url.path);
        if (!ws_.connect(parsed_url.host, parsed_url.port, parsed_url.path)) {
            WK_ERROR("Connection failed.");
            return false;
        }
        WK_INFO("Connected successfully.");
        return true;
    }

/*
    ----------------------------------------------------------------------------
    RATIONALE: Why subscribe()/unsubscribe() now take schema request types
    ----------------------------------------------------------------------------

    We originally exposed multiple overloads such as:
        subscribe_trade(const std::string&)
        subscribe_trade(const std::vector<std::string>&)
        subscribe_trade(const std::string&, uint64_t req_id)
        ...

    This quickly became unmaintainable and error-prone:
      - Overload explosion (8+ overloads for each subscription type).
      - Hard for developers to remember all combinations.
      - Future API expansions (adding snapshot flags, tags, metadata, etc.)
        would require even more overloads.

    The new design accepts a single argument:
        subscribe(const schema::trade::Subscribe& req);

    Benefits:
      • Strong type safety — each schema struct matches exactly the official
        Kraken WS schema for that request type.
      • Self-documenting — the struct fields convey all available options.
      • Extensible — adding new fields in the future does NOT require new
        overloads or breaking existing code.
      • Cleaner API — developers construct the request type they need and pass it.
      • Flexible req_id management — the caller may provide req_id or allow the
        client to auto-assign one.

    Example usage:
        client.subscribe(schema::trade::Subscribe{
            .symbols = {"BTC/USD"},
            .snapshot = false
        });

    Internally, requests are taken by value to allow efficient move semantics.
    This means passing temporaries incurs no copying overhead.

    In short:
        → Schema types prevent API bloat,
        → provide clarity,
        → and guarantee compatibility with Kraken’s evolving message formats.

    ----------------------------------------------------------------------------
    PERFORMANCE & API DESIGN NOTE:
    ----------------------------------------------------------------------------

    subscribe() and unsubscribe() accept schema::trade::Subscribe and
    Unsubscribe **by const-reference**, but internally we forward these
    to subscribe_with_ack_(), which accepts the request **by value**.

    Why?
    ----
    subscribe_with_ack_() must be allowed to *modify* the request before sending it,
    specifically:
        - auto-assigning req_id when the user has not provided one.

    The current design allows modify the request by:

        • subscribe(req)       → req passed by const&
        • subscribe_with_ack_(req)  → req copied once (or moved), then modified

    Move efficiency
    ---------------
    subscribe_with_ack_ takes the request **by value**, which enables the "pass-by-
    value + move when possible" modern C++ optimization pattern:

        • The temporary struct is moved into subscribe_with_ack_ (no heavy copies)
        • std::vector<std::string> and optional<T> are moved → O(1)
        • req_id can be safely auto-filled inside subscribe_with_ack_

    Copy only happens when the user passes a named lvalue — which is rare and
    intentional (the user explicitly wants to keep control over the request).

    Summary
    -------
    ✓ subscribe() and unsubscribe() stay clean and type-safe  
    ✓ subscribe_with_ack_() can mutate req_id safely  
    ✓ rvalue requests are moved → zero-copy fast path  
    ✓ lvalue requests copy once → intentional, explicit behavior  
    ✓ Schema structs eliminate overload explosion and future-proof the SDK

    This pattern is standard in modern high-performance SDK design.
    ----------------------------------------------------------------------------
*/

    template<class RequestT, class Callback>
    void subscribe(const RequestT& req, Callback&& cb) {
        using ResponseT = typename channel_traits<RequestT>::response_type;
        static_assert(requires { req.symbols; }, "Request must expose a member called `symbols`");
        // 1) Store callback safely once
        using StoredCallback = std::function<void(const ResponseT&)>;
        StoredCallback cb_copy = std::forward<Callback>(cb);
        // Register callback for the symbol(s)
        for (const auto& symbol : req.symbols) {
            dispatcher_.add_handler<ResponseT>(symbol, cb_copy);
        }
        subscribe_with_ack_(req, cb_copy);
    }

    inline void unsubscribe(const schema::trade::Unsubscribe& req) {
        unsubscribe_with_ack_(req);
    }

    // Main thread polling
    inline void poll() {
        { // === Process trade subscribe ring ===
        schema::trade::SubscribeAck ack;
        while (trade_subscribe_ring_.pop(ack)) {
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SUBMGR] Subscription ACK missing req_id for channel 'trade' {" << ack.symbol << "}");
                return;
            }
            trade_channel_manager_.process_subscribe_ack(ack.req_id.value(), ack.symbol, ack.success);
        }}
        { // === Process trade unsubscribe ring ===
        schema::trade::UnsubscribeAck ack;
        while (trade_unsubscribe_ring_.pop(ack)) {
            dispatcher_.remove_symbol_handlers<schema::trade::UnsubscribeAck>(ack.symbol);
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SUBMGR] Unsubscription ACK missing req_id for channel 'trade' {" << ack.symbol << "}");
                return;
            }
            trade_channel_manager_.process_unsubscribe_ack(ack.req_id.value(), ack.symbol, ack.success);
        }}
        { // === Process trade ring ===
        schema::trade::Response resp;
        while (trade_ring_.pop(resp)) {
            dispatcher_.dispatch(resp);
        }}
        // Add rings for orderbook, ticker, system status, etc.
    }

    // Accessor to the heartbeat counter
    [[nodiscard]] inline uint64_t heartbeat_total() const noexcept {
        return heartbeat_total_.load(std::memory_order_relaxed);
    }

    // Accessor to the subscription manager
    const channel::Manager& trade_subscriptions() const noexcept {
        return trade_channel_manager_;
    }

    bool reconnect() {
        // 1) Close old WS
        ws_.close();
        // 2) Clear runtime state
        dispatcher_.clear();
        trade_channel_manager_.clear_all();
        // 3) Attempt reconnection
        if (!connect(last_url_)) {
            return false;
        }
        WK_INFO("Connection re-established with server '" << last_url_ << "'. Replaying active subscriptions...");
        // 4) Replay all subscriptions
        auto trade_subscriptions = replay_db_.take_subscriptions<schema::trade::Subscribe>();
        for (const auto& subscription : trade_subscriptions) {
            subscribe(subscription.request(), subscription.callback());
        }
        return true;
    }


private:
    std::string last_url_;
    WS ws_;

    lcr::sequence req_id_seq_{};

    std::atomic<uint64_t> heartbeat_total_;
    lcr::lockfree::spsc_ring<schema::trade::Response, 4096> trade_ring_{};
    lcr::lockfree::spsc_ring<schema::trade::SubscribeAck, 16> trade_subscribe_ring_{};
    lcr::lockfree::spsc_ring<schema::trade::UnsubscribeAck, 16> trade_unsubscribe_ring_{};

    Parser parser_;
    Dispatcher dispatcher_;

    channel::Manager trade_channel_manager_;

    replay::Database replay_db_;

private:
    struct ParsedUrl {
        std::string scheme;
        std::string host;
        std::string port;
        std::string path;
    };

    // Very small URL parser supporting ws:// and wss://
    // Example inputs:
    //   wss://ws.kraken.com/v2
    //   ws://example.com:8080/stream
    inline ParsedUrl parse_url_(const std::string& url) {
        ParsedUrl out;
        // 1) Extract scheme
        const std::string ws  = "ws://";
        const std::string wss = "wss://";
        size_t pos = 0;
        if (url.rfind(ws, 0) == 0) {
            out.scheme = "ws";
            pos = ws.size();
        }
        else if (url.rfind(wss, 0) == 0) {
            out.scheme = "wss";
            pos = wss.size();
        }
        else {
            throw std::runtime_error("Unsupported URL scheme: " + url);
        }
        // 2) Extract host[:port]
        size_t slash = url.find('/', pos);
        std::string hostport = (slash == std::string::npos) ? url.substr(pos) : url.substr(pos, slash - pos);
        // 3) Split host and port
        size_t colon = hostport.find(':');
        if (colon != std::string::npos) {
            out.host = hostport.substr(0, colon);
            out.port = hostport.substr(colon + 1);
        } else {
            out.host = hostport;
            out.port = (out.scheme == "wss") ? "443" : "80";
        }
        // 4) Path
        out.path = (slash == std::string::npos) ? "/" : url.substr(slash);
        return out;
    }

    template<class RequestT, class Callback>
    inline void subscribe_with_ack_(RequestT req, Callback&& cb) {
        using ResponseT = typename channel_traits<RequestT>::response_type;
        using StoredCallback = std::function<void(const ResponseT&)>;
        // 1) Assign req_id if missing
        if (!req.req_id.has()) {
            req.req_id = req_id_seq_.next();
        }
        WK_INFO("Subscribing to channel '" << channel_name_of_v<RequestT> << "' " << to_string(req.symbols) << " with req_id=" << lcr::to_string(req.req_id));
        // 2) Store callback safely once and register in replay DB
        StoredCallback cb_copy = std::forward<Callback>(cb);
        replay_db_.add(req, cb_copy);
        // 3) Send JSON BEFORE moving req.symbols
        if (!ws_.send(req.to_json())) {
            WK_ERROR("Failed to send subscription request for req_id=" << lcr::to_string(req.req_id));
            return;
        }
        // 4) Tell subscription manager we are awaiting an ACK (transfer ownership of symbols)
        trade_channel_manager_.register_subscription(
            std::move(req.symbols),
            req.req_id.value()
        );
    }

    template<class RequestT>
    inline void unsubscribe_with_ack_(RequestT req) {
        // 1) Assign req_id if missing
        if (!req.req_id.has()) {
            req.req_id = req_id_seq_.next();
        }
        WK_INFO("Unsubscribing from channel '" << channel_name_of_v<RequestT> << "' " << to_string(req.symbols) << " with req_id=" << lcr::to_string(req.req_id));
        // 2) Register in replay DB (no callback needed for unsubscription)
        replay_db_.remove(req);
        // 3) Send JSON BEFORE moving req.symbols
        if (!ws_.send(req.to_json())) {
            WK_ERROR("Failed to send unsubscription request for req_id=" << lcr::to_string(req.req_id));
            return;
        }
        // 4) Tell subscription manager we are awaiting an ACK (transfer ownership of symbols)
        trade_channel_manager_.register_unsubscription(
            std::move(req.symbols),
            req.req_id.value()
        );
    }
};

} // namespace wirekrak

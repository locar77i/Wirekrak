#pragma once

#include <string>
#include <thread>

#include "wirekrak/winhttp/websocket.hpp"
#include "wirekrak/protocol/kraken/parser/context.hpp"
#include "wirekrak/protocol/kraken/parser/router.hpp"
#include "wirekrak/dispatcher.hpp"
#include "wirekrak/channel/manager.hpp"
#include "wirekrak/protocol/kraken/system/ping.hpp"
#include "wirekrak/protocol/kraken/trade/subscribe.hpp"
#include "wirekrak/protocol/kraken/trade/unsubscribe.hpp"
#include "wirekrak/protocol/kraken/trade/response.hpp"
#include "wirekrak/protocol/kraken/channel_traits.hpp"
#include "wirekrak/protocol/kraken/request/concepts.hpp"
#include "wirekrak/replay/database.hpp"
#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/transport/websocket.hpp"
#include "lcr/log/logger.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/sequence.hpp"

using namespace wirekrak::protocol::kraken;


namespace wirekrak {

// WireKrak uses a state-machine-driven reconnection model.
//
// Transport failures are detected at the WebSocket layer and handled deterministically in the client poll loop.
// All subscriptions are replayed automatically with exponential backoff.”
//
// The current design achieves:
// - Transport failure detection
// - Automatic reconnection
// - Subscription replay preserved
// - Exponential backoff implemented
// - Heartbeat-based liveness detection
// - Clean transport boundary
// - Deterministic poll-driven design
// - No extra threads (beyond the WebSocket's own receive thread)
//
// This is exactly how modern low-latency SDKs work.
//
// Besides, the heartbeats count is used as deterministic liveness signal that drives reconnection.
// Heartbeat timeout is NOT a transport concern. It is a protocol / client liveness concern.
template<transport::WebSocket WS>
class Client {
    static constexpr auto HEARTBEAT_TIMEOUT = std::chrono::seconds(10);
    static constexpr auto MESSAGE_TIMEOUT   = std::chrono::seconds(15);

public:
    using status_handler_t = std::function<void(const status::Update&)>;
    using pong_handler_t = std::function<void(const system::Pong&)>;

public:
    Client()
        : parser_(parser::Context{ .heartbeat_total = &heartbeat_total_,
                                   .last_heartbeat_ts = &last_heartbeat_ts_,
                                   .pong_ring = &pong_ring_,
                                    .status_ring = &status_ring_,
                                   .trade_ring = &trade_ring_,
                                   .trade_subscribe_ring = &trade_subscribe_ring_,
                                   .trade_unsubscribe_ring = &trade_unsubscribe_ring_,
                                   .book_ring = &book_ring_,
                                   .book_subscribe_ring = &book_subscribe_ring_,
                                   .book_unsubscribe_ring = &book_unsubscribe_ring_}) {
        last_heartbeat_ts_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        last_message_ts_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        ws_.set_message_callback([this](const std::string& msg){
            on_message_received_(msg);
        });
        ws_.set_close_callback([this]() {
            on_transport_closed_();
        });
    }

    ~Client() {
        ws_.close();
    }

    [[nodiscard]] inline bool connect(const std::string& url) {
        last_url_ = url;
        state_ = ConnState::Connecting;
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
            state_ = ConnState::Disconnected;
            WK_ERROR("Connection failed.");
            return false;
        }
        state_ = ConnState::Connected;
        retry_attempts_ = 0;
        WK_INFO("Connected successfully.");
        return true;
    }

    // Send ping
    inline void ping(lcr::optional<std::uint64_t> req_id = {}) noexcept{
        system::Ping ping{.req_id = req_id};
        send_raw_request_(system::Ping{.req_id = req_id});
    }

    // Register status callback
    inline void on_status(status_handler_t cb) noexcept {
        status_handler_ = std::move(cb);
    }

    // Register pong callback
    inline void on_pong(pong_handler_t cb) noexcept {
        pong_handler_ = std::move(cb);
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
        subscribe(const trade::Subscribe& req);

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
        client.subscribe(trade::Subscribe{
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

    subscribe() and unsubscribe() accept trade::Subscribe and
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

    template <request::Subscription RequestT, class Callback>
    inline void subscribe(const RequestT& req, Callback&& cb) {
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

    template <request::Unsubscription RequestT>
    inline void unsubscribe(const RequestT& req) {
        unsubscribe_with_ack_(req);
    }

    // Main thread polling
    inline void poll() {
        auto now = std::chrono::steady_clock::now();
        // === Heartbeat liveness check ===
        if (state_ == ConnState::Connected) {
            auto last_msg = last_message_ts_.load(std::memory_order_relaxed);
            bool message_stale   = (now - last_msg) > MESSAGE_TIMEOUT;
            auto last_hb  = last_heartbeat_ts_.load(std::memory_order_relaxed);
            bool heartbeat_stale = (now - last_hb)  > HEARTBEAT_TIMEOUT;
            // Conservative: only reconnect if BOTH are stale
            if (message_stale && heartbeat_stale) {
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_hb);
                WK_WARN("Heartbeat timeout (" << duration.count() << " ms). Forcing reconnect.");
                // Force transport failure → triggers reconnection
                ws_.close();
            }
        }
        // === Reconnection logic ===
        if (state_ == ConnState::WaitingReconnect && now >= next_retry_) {
            WK_INFO("Attempting reconnection...");
            if (reconnect_()) {
                WK_INFO("Reconnected successfully");
                state_ = ConnState::Connected;
            } else {
                retry_attempts_++;
                next_retry_ = now + backoff_(retry_attempts_);
            }
        }
        // ===============================================================================
        // PROCESS STATUS MESSAGES
        // ===============================================================================
        { // === Process status ring ===
        status::Update update;
        while (status_ring_.pop(update)) {
            handle_status_(update);
        }}
        // ===============================================================================
        // PROCESS PONG MESSAGES
        // ===============================================================================
        { // === Process pong ring ===
        system::Pong pong;
        while (pong_ring_.pop(pong)) {
            handle_pong_(pong);
        }}
        // ===============================================================================
        // PROCESS TRADE MESSAGES
        // ===============================================================================
        { // === Process trade ring ===
        trade::Response resp;
        while (trade_ring_.pop(resp)) {
            for (auto& trade_msg : resp.trades) {
                dispatcher_.dispatch(trade_msg);
            }
        }}
        { // === Process trade subscribe ring ===
        trade::SubscribeAck ack;
        while (trade_subscribe_ring_.pop(ack)) {
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SUBMGR] Subscription ACK missing req_id for channel 'trade' {" << ack.symbol << "}");
                return;
            }
            trade_channel_manager_.process_subscribe_ack(Channel::Trade, ack.req_id.value(), ack.symbol, ack.success);
        }}
        { // === Process trade unsubscribe ring ===
        trade::UnsubscribeAck ack;
        while (trade_unsubscribe_ring_.pop(ack)) {
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
        book::Update resp;
        while (book_ring_.pop(resp)) {
            dispatcher_.dispatch(resp);
        }}
        { // === Process book subscribe ring ===
        book::SubscribeAck ack;
        while (book_subscribe_ring_.pop(ack)) {
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SUBMGR] Subscription ACK missing req_id for channel 'book' {" << ack.symbol << "}");
                return;
            }
            book_channel_manager_.process_subscribe_ack(Channel::Book, ack.req_id.value(), ack.symbol, ack.success);
        }}
        { // === Process book unsubscribe ring ===
        book::UnsubscribeAck ack;
        while (book_unsubscribe_ring_.pop(ack)) {
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
        return heartbeat_total_.load(std::memory_order_relaxed);
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

#ifdef WK_UNIT_TEST
    void force_last_message(std::chrono::steady_clock::time_point ts) {
        last_message_ts_.store(ts, std::memory_order_relaxed);
    }

    void force_last_heartbeat(std::chrono::steady_clock::time_point ts) {
        last_heartbeat_ts_.store(ts, std::memory_order_relaxed);
    }

    WS& ws() {
        return ws_;
    }
#endif // WK_UNIT_TEST

private:
    std::string last_url_;
    WS ws_;

    lcr::sequence req_id_seq_{};

    // The kraken heartbeats count is used as deterministic liveness signal that drives reconnection.
    // If no heartbeat is received for N seconds:
    // - Assume the connection is unhealthy (even if TCP is still “up”)
    // - Force-close the WebSocket
    // - Let the existing reconnection state machine recover
    // - Replay subscriptions automatically
    //
    // Benefits:
    // - Simple liveness detection
    // - Decouples transport health from protocol health
    // - No threads. No timers. Poll-driven.
    std::atomic<uint64_t> heartbeat_total_;
    std::atomic<std::chrono::steady_clock::time_point> last_heartbeat_ts_;
    std::atomic<std::chrono::steady_clock::time_point> last_message_ts_;

    // Status callback
    status_handler_t status_handler_;
    // Pong callback
    pong_handler_t pong_handler_;

    // Output rings for pong messages
    lcr::lockfree::spsc_ring<system::Pong, 8> pong_ring_{};

    // Output rings for status channel
    lcr::lockfree::spsc_ring<status::Update, 8> status_ring_{};

    // Output rings for trade channel
    lcr::lockfree::spsc_ring<trade::Response, 4096> trade_ring_{};
    lcr::lockfree::spsc_ring<trade::SubscribeAck, 8> trade_subscribe_ring_{};
    lcr::lockfree::spsc_ring<trade::UnsubscribeAck, 8> trade_unsubscribe_ring_{};

    // Output rings for book channel
    lcr::lockfree::spsc_ring<book::Update, 4096> book_ring_{};
    lcr::lockfree::spsc_ring<book::SubscribeAck, 8> book_subscribe_ring_{};
    lcr::lockfree::spsc_ring<book::UnsubscribeAck, 8> book_unsubscribe_ring_{};

    parser::Router parser_;
    Dispatcher dispatcher_;

    channel::Manager trade_channel_manager_;
    channel::Manager book_channel_manager_;

    replay::Database replay_db_;

private:
    enum class ConnState {
        Disconnected,
        Connecting,
        Connected,
        WaitingReconnect
    };

    ConnState state_ = ConnState::Disconnected;
    std::chrono::steady_clock::time_point next_retry_;
    int retry_attempts_ = 0;

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

    inline void on_message_received_(const std::string& msg) { // Placeholder for user-defined behavior on message receipt
        last_message_ts_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        parser_.parse_and_route(msg);
    }

    inline void on_transport_closed_() { // Placeholder for user-defined behavior on transport closure
        WK_DEBUG("WebSocket closed");
        if (state_ == ConnState::Connected) {
            state_ = ConnState::WaitingReconnect;
            retry_attempts_++;
            next_retry_ = std::chrono::steady_clock::now() + backoff_(retry_attempts_);
        }
    }

    inline void handle_status_(const status::Update& status) noexcept {
        if (status_handler_) {
            status_handler_(status);
        }
    }

    inline void handle_pong_(const system::Pong& pong) noexcept {
        if (pong_handler_) {
            pong_handler_(pong);
        }
    }

    [[nodiscard]]
    inline bool reconnect_() {
        // 1) Close old WS
        ws_.close();
        // 2) Clear runtime state
        dispatcher_.clear();
        trade_channel_manager_.clear_all();
        book_channel_manager_.clear_all();
        // 3) Attempt reconnection
        if (!connect(last_url_)) {
            return false;
        }
        WK_INFO("Connection re-established with server '" << last_url_ << "'. Replaying active subscriptions...");
        // 4) Replay all subscriptions
        auto trade_subscriptions = replay_db_.take_subscriptions<trade::Subscribe>();
        for (const auto& subscription : trade_subscriptions) {
            subscribe(subscription.request(), subscription.callback());
        }
        return true;
    }

    inline std::chrono::milliseconds backoff_(int attempt) {
        using namespace std::chrono;
        return std::min(
            milliseconds(100 * (1 << attempt)),
            milliseconds(5000)
        );
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
        // 1) Assign req_id if missing
        if (!req.req_id.has()) {
            req.req_id = req_id_seq_.next();
        }
        std::string json = req.to_json();
        if (!ws_.send(json)) {
            WK_ERROR("Failed to send raw message: " << json);
        }
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
        subscription_manager_for_<RequestT>().register_subscription(
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
        subscription_manager_for_<RequestT>().register_unsubscription(
            std::move(req.symbols),
            req.req_id.value()
        );
    }
};

} // namespace wirekrak

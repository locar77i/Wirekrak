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

Data-plane model:
  - Core exposes protocol messages exactly as received
  - Messages are delivered via bounded SPSC rings
  - No callbacks, observers, or implicit dispatch
  - Consumers explicitly pull or drain messages after poll()
===============================================================================
*/

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
#include "wirekrak/core/protocol/policy/symbol_limit.hpp"
#include "wirekrak/core/protocol/kraken/context.hpp"
#include "wirekrak/core/protocol/kraken/request/concepts.hpp"
#include "wirekrak/core/protocol/kraken/schema/system/ping.hpp"
#include "wirekrak/core/protocol/kraken/channel_traits.hpp"
#include "wirekrak/core/protocol/kraken/request/concepts.hpp"
#include "wirekrak/core/protocol/kraken/parser/router.hpp"
#include "wirekrak/core/protocol/kraken/channel/manager.hpp"
#include "wirekrak/core/protocol/kraken/replay/database.hpp"
#include "lcr/log/logger.hpp"
#include "lcr/local/ring_buffer.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/sequence.hpp"


namespace wirekrak::core {
namespace protocol {
namespace kraken {

template<
    transport::WebSocketConcept WS,
    policy::SymbolLimitConcept LimitPolicy = policy::NoSymbolLimits
>
class Session {

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

    // -----------------------------------------------------------------------------
    // Pong message access (last-value, pull-based)
    //
    // The Session exposes the most recently received Pong message exactly as
    // provided by the exchange schema.
    //
    // Pong is treated as **state**, not a stream, and is exposed as a last-value fact:
    //   • Only the latest Pong is retained
    //   • Intermediate Pong messages may be overwritten
    //   • No backpressure or buffering is applied
    //
    // Consumers explicitly poll for updates and decide when to observe changes.
    //
    // Threading & semantics:
    //   • Safe for concurrent readers
    //   • Change detection is per-calling thread
    //   • No guarantee that every Pong will be observed
    //
    // -----------------------------------------------------------------------------
    [[nodiscard]]
    inline bool try_load_pong(schema::system::Pong& out) noexcept {
        return ctx_.pong_slot.try_load(out);
    }

    // -----------------------------------------------------------------------------
    // Status message access (last-value, pull-based)
    //
    // The Session exposes the most recently received Status message exactly as
    // provided by the exchange schema.
    //
    // Status is treated as **state**, not a stream:
    //   • Only the latest Status is retained
    //   • Intermediate Status messages may be overwritten
    //   • No backpressure or buffering is applied
    //
    // Consumers explicitly poll for updates and decide when to observe changes.
    //
    // Threading & semantics:
    //   • Safe for concurrent readers
    //   • Change detection is per-calling thread
    //   • No guarantee that every Status will be observed
    //
    // -----------------------------------------------------------------------------
    [[nodiscard]]
    inline bool try_load_status(schema::status::Update& out) noexcept {
        return ctx_.status_slot.try_load(out);
    }

    // -----------------------------------------------------------------------------
    // Rejection message access
    // -----------------------------------------------------------------------------
    // NOTE:
    // Rejection messages MUST be drained by the user.
    // Failure to do so is considered a protocol-handling error and will
    // eventually force the session to close defensively.

    [[nodiscard]]
    inline bool pop_rejection(schema::rejection::Notice& out) noexcept {
        return user_rejection_buffer_.pop(out);
    }

    // Convenience method to drain all available messages with a user-provided callback
    template<class F>
    void drain_rejection_messages(F&& f) noexcept(noexcept(f(std::declval<const schema::rejection::Notice&>()))) {
        schema::rejection::Notice msg;
        while (user_rejection_buffer_.pop(msg)) {
            std::forward<F>(f)(msg);
        }
    }

    // -----------------------------------------------------------------------------
    // Trade message access
    // -----------------------------------------------------------------------------
    // NOTE:
    // Returned Response objects are owned by the Session rings.
    // They remain valid only until overwritten by subsequent pops.
    // Users must not retain references beyond the call scope.

    [[nodiscard]]
    inline bool pop_trade_message(schema::trade::Response& out) noexcept {
        return ctx_.trade_ring.pop(out);
    }

    // Convenience method to drain all available messages with a user-provided callback
    template<class F>
    void drain_trade_messages(F&& f) noexcept(noexcept(f(std::declval<const schema::trade::Response&>()))) {
        schema::trade::Response msg;
        while (ctx_.trade_ring.pop(msg)) {
            std::forward<F>(f)(msg);
        }
    }

    // -----------------------------------------------------------------------------
    // Book message access
    // -----------------------------------------------------------------------------
    // NOTE:
    // Returned Response objects are owned by the Session rings.
    // They remain valid only until overwritten by subsequent pops.
    // Users must not retain references beyond the call scope.

    [[nodiscard]]
    inline bool pop_book_message(schema::book::Response& out) noexcept {
        return ctx_.book_ring.pop(out);
    }

    // Convenience method to drain all available messages with a user-provided callback
    template<class F>
    void drain_book_messages(F&& f) noexcept(noexcept(f(std::declval<const schema::book::Response&>()))) {
        schema::book::Response msg;
        while (ctx_.book_ring.pop(msg)) {
            std::forward<F>(f)(msg);
        }
    }

    // -----------------------------------------------------------------------------
    // Control-plane messages
    // -----------------------------------------------------------------------------

    inline void ping() noexcept{
        send_raw_request_(schema::system::Ping{.req_id = ctrl::PING_ID});
    }

    template <request::Subscription RequestT>
    inline ctrl::req_id_t subscribe(RequestT req) {
        static_assert(request::ValidRequestIntent<RequestT>,
            "Invalid request type: a request must define exactly one intent tag (subscribe_tag, unsubscribe_tag, control_tag...)"
        );
        static_assert(requires { req.symbols; }, "Request must expose a member called `symbols`");
        WK_INFO("Subscribing to channel '" << channel_name_of_v<RequestT> << "' " << core::to_string(req.symbols));
        // 1) Hard symbol limit enforcement (compile-time removable)
        if constexpr (LimitPolicy::enabled && LimitPolicy::hard) {
            if (!hard_symbol_limit_enforcement_<RequestT>(req)) {
                return ctrl::INVALID_REQ_ID;
            }
        }
        // 2) Assign req_id if missing
        if (!req.req_id.has()) {
            req.req_id = req_id_seq_.next();
        }
        // 3) Register in replay DB
        // Store protocol intent for deterministic replay after reconnect.
        // Only acknowledged subscriptions will be replayed.
        replay_db_.add(req);
        // 4) Send JSON BEFORE moving req.symbols
        WK_DEBUG("Sending subscribe message: " << req.to_json());
        if (!connection_.send(req.to_json())) {
            WK_ERROR("Failed to send subscription request for req_id=" << lcr::to_string(req.req_id));
            return ctrl::INVALID_REQ_ID;
        }
        // 5) Tell subscription manager we are awaiting an ACK (transfer ownership of symbols)
        subscription_manager_for_<RequestT>().register_subscription(
            std::move(req.symbols),
            req.req_id.value()
        );
        return req.req_id.value();
    }

    // ------------------------------------------------------------
    // Hard symbol limit enforcement
    // ------------------------------------------------------------
    template <request::Subscription RequestT>
    [[nodiscard]]
    inline bool hard_symbol_limit_enforcement_(const RequestT& req) const noexcept {
        const std::size_t requested = req.symbols.size();
        // Current logical symbol counts
        const std::size_t trade_now = trade_channel_manager_.total_symbols();
        const std::size_t book_now = book_channel_manager_.total_symbols();
        const std::size_t global_now = trade_now + book_now;
        // Check limits
        if constexpr (channel_of_v<RequestT> == Channel::Trade) { // Check trade limits

            if (LimitPolicy::max_trade > 0 && trade_now + requested > LimitPolicy::max_trade) {
                WK_WARN("[SESSION] Trade symbol limit exceeded (" << trade_now + requested << " > " << LimitPolicy::max_trade << ")");
                return false;
            }
        }
        else if constexpr (channel_of_v<RequestT> == Channel::Book) { // Check book limits

            if (LimitPolicy::max_book > 0 && book_now + requested > LimitPolicy::max_book) {
                WK_WARN("[SESSION] Book symbol limit exceeded (" << book_now + requested << " > " << LimitPolicy::max_book << ")");
                return false;
            }
        }
        // Check global limits
        if (LimitPolicy::max_global > 0 && global_now + requested > LimitPolicy::max_global) {
            WK_WARN("[SESSION] Global symbol limit exceeded (" << global_now + requested << " > " << LimitPolicy::max_global << ")");
            return false;
        }
        return true;
    }


    template <request::Unsubscription RequestT>
    inline ctrl::req_id_t unsubscribe(RequestT req) {
        static_assert(request::ValidRequestIntent<RequestT>,
            "Invalid request type: a request must define exactly one intent tag (subscribe_tag, unsubscribe_tag, control_tag...)"
        );
         WK_INFO("Unsubscribing from channel '" << channel_name_of_v<RequestT> << "' " << core::to_string(req.symbols));
        // 1) Assign req_id if missing
        if (!req.req_id.has()) {
            req.req_id = req_id_seq_.next();
        }
        // 2) Send JSON BEFORE moving req.symbols
        WK_DEBUG("Sending unsubscribe message: " << req.to_json());
        if (!connection_.send(req.to_json())) {
            WK_ERROR("Failed to send unsubscription request for req_id=" << lcr::to_string(req.req_id));
            return ctrl::INVALID_REQ_ID;
        }
        // 3) Tell subscription manager we are awaiting an ACK (transfer ownership of symbols)
        subscription_manager_for_<RequestT>().register_unsubscription(
            std::move(req.symbols),
            req.req_id.value()
        );
        return req.req_id.value();
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
    //
    // NOTE:
    // poll() must be called to advance the session and populate message rings.
    // Calling pop_* or drain_* without polling will not make progress.
    //
    // NOTE:
    // Trade and book message rings are not drained here.
    // They are exposed verbatim to the user via pop_* / drain_* methods.
    //
    // Ordering guarantee:
    //   - poll() processes control-plane events before data-plane delivery
    //   - ACKs and rejections are handled before user-visible messages are drained
    [[nodiscard]]
    inline std::uint64_t poll() {
        // === Heartbeat liveness & reconnection logic ===
        connection_.poll();
        transport::connection::Signal sig;
        while (connection_.poll_signal(sig)) {
            handle_connection_signal_(sig);
        }
        
        // ===============================================================================
        // PROCESS REJECTION NOTICES (lossless, semantic errors)
        // ===============================================================================
        // Rejection notices represent protocol-level failures and MUST NOT be dropped.
        // Failure to drain rejections is a user error and indicates that protocol
        // correctness can no longer be guaranteed.
        //
        // Core processes rejections internally for correctness, then exposes them
        // losslessly to the user via pop_rejection().
        // ===============================================================================
        { // === Process rejection ring ===
        schema::rejection::Notice notice;
        while (ctx_.rejection_ring.pop(notice)) {
            // 1) Apply internal protocol correctness handling
            handle_rejection_(notice);
            // 2) Forward to user-visible rejection buffer (lossless)
            if (!user_rejection_buffer_.push(notice)) [[unlikely]] { // This is a hard failure: we cannot report semantic errors reliably
                WK_FATAL("[SESSION] Rejection buffer overflow — protocol correctness compromised (user not draining rejections)");
                // Defensive action: close the connection to prevent further damage
                connection_.close();
                break;
            }
        }}
        
        // ===============================================================================
        // PROCESS TRADE MESSAGES
        // ===============================================================================
        { // === Process trade subscribe ring ===
        schema::trade::SubscribeAck ack;
        while (ctx_.trade_subscribe_ring.pop(ack)) {
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SUBMGR] Subscription ACK missing req_id for channel 'trade' {" << ack.symbol << "}");
            }
            else {
                trade_channel_manager_.process_subscribe_ack(ack.req_id.value(), ack.symbol, ack.success);
            }
        }}
        { // === Process trade unsubscribe ring ===
        schema::trade::UnsubscribeAck ack;
        while (ctx_.trade_unsubscribe_ring.pop(ack)) {
            WK_TRACE("[SUBMGR] Processing trade unsubscribe ACK for symbol {" << ack.symbol << "}");
            //dispatcher_.remove_symbol_handlers<schema::trade::UnsubscribeAck>(ack.symbol);
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SUBMGR] Unsubscription ACK missing req_id for channel 'trade' {" << ack.symbol << "}");
            }
            else {
                trade_channel_manager_.process_unsubscribe_ack(ack.req_id.value(), ack.symbol, ack.success);
                if (ack.success) {
                    replay_db_.remove_symbol<schema::trade::Subscribe>(ack.symbol);
                }
            }
        }}
        // ===============================================================================
        // PROCESS BOOK UPDATES
        // ===============================================================================
        { // === Process book subscribe ring ===
        schema::book::SubscribeAck ack;
        while (ctx_.book_subscribe_ring.pop(ack)) {
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SUBMGR] Subscription ACK missing req_id for channel 'book' {" << ack.symbol << "}");
            }
            else {
                book_channel_manager_.process_subscribe_ack(ack.req_id.value(), ack.symbol, ack.success);
            }
        }}
        { // === Process book unsubscribe ring ===
        schema::book::UnsubscribeAck ack;
        while (ctx_.book_unsubscribe_ring.pop(ack)) {
            WK_TRACE("[SUBMGR] Processing book unsubscribe ACK for symbol {" << ack.symbol << "}");
            //dispatcher_.remove_symbol_handlers<schema::book::UnsubscribeAck>(ack.symbol);
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SUBMGR] Unsubscription ACK missing req_id for channel 'book' {" << ack.symbol << "}");
            }
            else {
                book_channel_manager_.process_unsubscribe_ack(ack.req_id.value(), ack.symbol, ack.success);
                if (ack.success) {
                    replay_db_.remove_symbol<schema::book::Subscribe>(ack.symbol);
                }
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
    inline std::uint64_t heartbeat_total() const noexcept {
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
    inline std::uint64_t transport_epoch() const noexcept {
        return connection_.epoch();
    }

    [[nodiscard]]
    inline std::uint64_t rx_messages() const noexcept {
        return connection_.rx_messages();
    }

    [[nodiscard]]
    inline std::uint64_t tx_messages() const noexcept {
        return connection_.tx_messages();
    }

    [[nodiscard]]
    inline std::uint64_t hb_messages() const noexcept {
        return connection_.hb_messages();
    }

    // -----------------------------------------------------------------------------
    // Protocol quiescence indicator
    // -----------------------------------------------------------------------------
    //
    // Returns true if the Session is **protocol-idle**.
    //
    // Protocol-idle means that, at the current instant:
    //   • No subscribe or unsubscribe requests are awaiting ACKs
    //   • No protocol replays, reconnect handshakes, or retry cycles are in progress
    //   • No control-plane work remains that requires further poll() calls
    //
    // In other words:
    //   If poll() is never called again, the Session will not violate
    //   protocol correctness or leave the exchange in an inconsistent state.
    //
    // IMPORTANT SEMANTICS:
    //
    // • This is NOT a data-plane signal.
    //   Active subscriptions may still exist and produce future data.
    //
    // • This does NOT guarantee that all user-visible messages
    //   (trade, book, rejection, status, pong) have been drained.
    //
    // • This does NOT imply that the transport is closed or inactive.
    //
    // Threading & usage:
    //   • Not thread-safe
    //   • Intended to be queried from the Session event loop
    //   • Typically used to drive graceful shutdown or drain loops
    //
    // -----------------------------------------------------------------------------
    // Example usage:
    //
    //   // Drain until protocol is quiescent
    //   while (!session.is_idle()) {
    //       session.poll();
    //       drain_messages();
    //   }
    //
    // -----------------------------------------------------------------------------
    [[nodiscard]]
    inline bool is_idle() const noexcept {
        return
            connection_.is_idle() &&
            ctx_.empty() &&
            user_rejection_buffer_.empty() &&
            !trade_channel_manager_.has_pending_requests() &&
            !book_channel_manager_.has_pending_requests();
    }

    [[nodiscard]]
    inline std::size_t pending_protocol_requests() const noexcept {
        return
            trade_channel_manager_.pending_requests() +
            book_channel_manager_.pending_requests();
    }

    [[nodiscard]]
    inline std::size_t pending_protocol_symbols() const noexcept {
        return
            trade_channel_manager_.pending_subscribe_symbols() +
            trade_channel_manager_.pending_unsubscribe_symbols() +
            book_channel_manager_.pending_subscribe_symbols() +
            book_channel_manager_.pending_unsubscribe_symbols();
    }

    [[nodiscard]]
    inline const replay::Database& replay_database() const noexcept {
        return replay_db_;
    }

#ifdef WK_UNIT_TEST
public:
        transport::Connection<WS>& connection() {
            return connection_;
        }

        WS& ws() {
            return connection_.ws();
        }
#endif // WK_UNIT_TEST

private:
    // Sequence generator for request IDs
    lcr::sequence req_id_seq_{ctrl::PROTOCOL_BASE};

    // Underlying streaming client (and telemetry)
    transport::telemetry::Connection telemetry_;
    transport::Connection<WS> connection_{telemetry_};

    // Liveness policy
    policy::Liveness liveness_policy_{policy::Liveness::Passive};

    // Session context (owning)
    Context ctx_;

    // Session context view (non-owning)
    ContextView ctx_view_;

    // Protocol parser / router
    parser::Router parser_;

    // User-visible rejection queue.
    // Decoupled from internal protocol processing to prevent user behavior from affecting Core correctness.
    lcr::local::ring_buffer<schema::rejection::Notice, config::rejection_ring> user_rejection_buffer_;

    // Channel subscription managers
    channel::Manager trade_channel_manager_{Channel::Trade};
    channel::Manager book_channel_manager_{Channel::Book};

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
                    (void)subscribe(subscription.request());
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
                    (void)subscribe(subscription.request());
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
        trade_channel_manager_.clear_all();
        book_channel_manager_.clear_all();
    }

    inline void handle_message_(std::string_view sv) {
        parser_.parse_and_route(sv);
    }

    inline void handle_rejection_(const schema::rejection::Notice& notice) noexcept {
        WK_TRACE("[SESSION] Handling rejection notice for symbol {" << (notice.symbol.has() ? notice.symbol.value() : "N/A" )
            << "} (req_id=" << (notice.req_id.has() ? notice.req_id.value() : ctrl::INVALID_REQ_ID) << ") - " << notice.error);
        if (notice.req_id.has()) {
            if (notice.symbol.has()) {
                // we cannot infer channel from notice, so we try all managers
                bool done = trade_channel_manager_.try_process_rejection(notice.req_id.value(), notice.symbol.value());
                if (!done) {
                    done = book_channel_manager_.try_process_rejection(notice.req_id.value(), notice.symbol.value());
                }
                done = replay_db_.try_process_rejection(notice.req_id.value(), notice.symbol.value());
            }
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
                    send_raw_request_(schema::system::Ping{.req_id = ctrl::PING_ID});
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
};


} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core


#pragma once

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

#include <string_view>
#include <functional>
#include <chrono>
#include <utility>
#include <ostream>
#include <memory>

#include "wirekrak/core/transport/websocket_concept.hpp"
#include "wirekrak/core/transport/connection.hpp"
#include "wirekrak/core/protocol/concept/json_writable.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"
#include "wirekrak/core/protocol/request/scheduler.hpp"
#include "wirekrak/core/protocol/telemetry/session.hpp"
#include "wirekrak/core/protocol/kraken/context.hpp"
#include "wirekrak/core/protocol/kraken/schema/system/ping.hpp"
#include "wirekrak/core/protocol/kraken/channel_traits.hpp"
#include "wirekrak/core/protocol/kraken/request/concepts.hpp"
#include "wirekrak/core/protocol/kraken/parser/router.hpp"
#include "wirekrak/core/protocol/kraken/channel/manager.hpp"
#include "wirekrak/core/protocol/kraken/replay/database.hpp"
#include "wirekrak/core/policy/protocol/session_bundle.hpp"
#include "wirekrak/core/policy/transport/connection_bundle.hpp"
#include "wirekrak/core/config/protocol.hpp"
#include "wirekrak/core/config/backpressure.hpp"
#include "lcr/memory/footprint.hpp"
#include "lcr/local/raw_buffer.hpp"
#include "lcr/local/queue.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/control/consecutive_state.hpp"
#include "lcr/buffer/concepts.hpp"
#include "lcr/sequence.hpp"
#include "lcr/metrics/util/scope_timer.hpp"
#include "lcr/system/thread_affinity.hpp"
#include "lcr/log/logger.hpp"
#include "lcr/trap.hpp"


namespace wirekrak::core::protocol::kraken {

template<
    transport::WebSocketConcept WS,
    lcr::buffer::ConsumerSpscRingConcept MessageRing,
    policy::protocol::SessionBundleConcept PolicyBundle = policy::protocol::DefaultSession,
    policy::transport::ConnectionBundleConcept ConnectionPolicyBundle = policy::transport::DefaultConnection
>
class Session {
public:
    using ConnectionT = transport::Connection<WS, MessageRing, ConnectionPolicyBundle>;

public:
    Session(MessageRing& ring)
        : telemetry_()
        , connection_(ring, telemetry_.connection)
        , ctx_(std::make_unique<Context>())
        , ctx_view_(*ctx_)
        , parser_(ctx_view_)
    {
        lcr::system::pin_thread(4); // Pin session thread to core 1 for deterministic performance  
    }

    // open connection
    [[nodiscard]]
    inline bool connect(std::string_view url) {
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
        if (ctx_->pong_slot.has()) [[likely]] {
            out = ctx_->pong_slot.value();
            ctx_->pong_slot.reset(); // Clear after reading to prevent stale data
            return true;
        }
        return false;
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
        if (ctx_->status_slot.has()) [[likely]] {
            out = ctx_->status_slot.value();
            ctx_->status_slot.reset(); // Clear after reading to prevent stale data
            return true;
        }
        return false;
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
        return ctx_->trade_ring.pop(out);
    }

    // Convenience method to drain all available messages with a user-provided callback
    template<class F>
    void drain_trade_messages(F&& f) noexcept(noexcept(f(std::declval<const schema::trade::Response&>()))) {
        schema::trade::Response msg;
        while (ctx_->trade_ring.pop(msg)) {
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
        return ctx_->book_ring.pop(out);
    }

    // Convenience method to drain all available messages with a user-provided callback
    template<class F>
    void drain_book_messages(F&& f) noexcept(noexcept(f(std::declval<const schema::book::Response&>()))) {
        schema::book::Response msg;
        while (ctx_->book_ring.pop(msg)) {
            std::forward<F>(f)(msg);
        }
    }

    // -----------------------------------------------------------------------------
    // Control-plane messages
    // -----------------------------------------------------------------------------

    inline void ping() noexcept{
        schema::system::Ping req{.req_id = ctrl::PING_ID};
        WK_DEBUG("[SESSION] Sending ping message: " << req.to_json());
        (void)send_request_(req);
    }

    template <request::Subscription RequestT>
    inline ctrl::req_id_t subscribe(RequestT req) {
        static_assert(request::ValidRequestIntent<RequestT>,
            "Invalid request type: a request must define exactly one intent tag (subscribe_tag, unsubscribe_tag, control_tag...)"
        );
        static_assert(requires { req.symbols; }, "Request must expose a member called `symbols`");
        //WK_INFO("[SESSION] Subscribing to channel '" << channel_name_of_v<RequestT> << "' " << core::to_string(req.symbols));
        WK_INFO("[SESSION] Subscribing to channel '" << channel_name_of_v<RequestT> << "' (total: " << req.symbols.size() << " symbol/s)");
        // 1) Hard symbol limit enforcement (compile-time removable)
        if constexpr (SymbolLimitPolicy::enabled && SymbolLimitPolicy::hard) {
            if (!hard_symbol_limit_enforcement_<RequestT>(req)) {
                WK_WARN("[SESSION:" << channel_name_of_v<RequestT> << "] Subscription rejected due to hard symbol limit enforcement (" << req.symbols.size() << " symbol/s omitted)");
                return ctrl::INVALID_REQ_ID;
            }
        }
        // 2) Assign req_id if missing
        if (!req.req_id.has()) {
            req.req_id = req_id_seq_.next();
        }
        // 3) Register subscription with manager (internal filtering)
        auto& mgr = subscription_manager_for_<RequestT>();
        auto accepted_symbols = mgr.register_subscription(std::move(req.symbols), req.req_id.value());
        if (accepted_symbols.empty()) {
            WK_TRACE("[SESSION] Subscription fully filtered by manager");
            return ctrl::INVALID_REQ_ID;
        }
        // 4) Replace request symbols with accepted set
        req.symbols = std::move(accepted_symbols);
        // 5) Register in replay DB using filtered request (only if replay enabled)
        if constexpr (ReplayPolicy::enabled) {
            // Store protocol intent for deterministic replay after reconnect.
            // Only acknowledged subscriptions will be replayed.
            replay_db_.add(req);
        }
        // 6) Emit the request according to the configured batching policy
        WK_DEBUG("[SESSION] Emitting subscribe message: " << req.symbols.size() << " symbol/s");
        if (!emit_subscription_(req)) {
            return ctrl::INVALID_REQ_ID;
        }
        WK_TL1(telemetry_.subscriptions_requested_total.inc());
        return req.req_id.value();
    }

    template <request::Unsubscription RequestT>
    inline ctrl::req_id_t unsubscribe(RequestT req) {
        static_assert(request::ValidRequestIntent<RequestT>,
            "Invalid request type: a request must define exactly one intent tag (subscribe_tag, unsubscribe_tag, control_tag...)"
        );
        //WK_INFO("[SESSION] Unsubscribing from channel '" << channel_name_of_v<RequestT> << "' " << core::to_string(req.symbols));
        WK_INFO("[SESSION] Unsubscribing from channel '" << channel_name_of_v<RequestT> << "' (total: " << req.symbols.size() << " symbol/s)");
        // 1) Assign req_id if missing
        if (!req.req_id.has()) {
            req.req_id = req_id_seq_.next();
        }
        // 2) Send JSON BEFORE moving req.symbols
        // TODO: Maybe we can add a policy to control whether JSON is generated before or after manager registration (which modifies the symbol list)?
        // For now, we send the user's original intent even if some symbols are filtered by the manager.
        WK_DEBUG("[SESSION] Sending unsubscribe message: " << req.symbols.size() << " symbol/s (req_id=" << req.req_id.value() << ")");
        if (!send_request_(req)) {
            return ctrl::INVALID_REQ_ID;
        }
        WK_TL1(telemetry_.unsubscriptions_requested_total.inc());
        // 3) Tell subscription manager we are awaiting an ACK (transfer ownership of symbols)
        RequestSymbols cancelled = subscription_manager_for_<RequestT>().register_unsubscription(std::move(req.symbols), req.req_id.value());
        // 4) Update replay DB to prevent replay of the cancelled symbols after reconnect (only if replay enabled)
        if constexpr (ReplayPolicy::enabled) {
            for (const auto& symbol : cancelled) {
                replay_db_.remove_symbol<RequestT>(symbol);
            }
        }
        return req.req_id.value();
    }

    // -----------------------------------------------------------------------------
    // NOTE
    // -----------------------------------------------------------------------------
    // The current poll() implementation explicitly drains each ring buffer in-line.
    // While functionally correct, this results in repetitive boilerplate and mixes
    // event-loop orchestration with message-specific handling.
    //
    // A future refactor should introduce a generic ring-draining helper and/or
    // group ring processing by domain (system, trade, book, etc.) to improve
    // readability, extensibility, and maintainability.
    //
    // This refactor was intentionally deferred to prioritize correctness and stability.
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
        std::size_t messages_processed = 0;
        auto& clock = lcr::system::monotonic_clock::instance();
        WK_TL3(
            lcr::metrics::util::scope_timer timer{telemetry_.poll_duration}
        );

        // === Advance transport state - Heartbeat liveness & reconnection logic ===
        connection_.poll();

        // Observability: track control plane pressure (ring size) at each poll
        WK_TL1(
            const std::size_t pending_signals = connection_.pending_signals();
             if (pending_signals > 0) [[likely]] {
                telemetry_.control_ring_depth.set(pending_signals);
            }
        );

        // === Drain transport control-plane signals ===
        transport::connection::Signal sig;
        while (connection_.poll_signal(sig)) {
            handle_connection_signal_(sig);
        }

        // Observability: track data plane pressure (ring size) at each poll
        WK_TL1(
            const std::size_t pending_messages = connection_.pending_messages();
             if (pending_messages > 0) [[likely]] {
                telemetry_.message_ring_depth.set(pending_messages);
            }
        );

        // === Drain transport data-plane (zero-copy) ===
        while (messages_processed < config::protocol::MAX_MESSAGES_PER_POLL) {
            auto* slot = connection_.peek_message();
            if (!slot) [[unlikely]] { // No more messages to process
/*
                const unsigned int spin_until = (config::protocol::MAX_MESSAGES_PER_POLL - messages_processed) * 3;
                for (int i = 0; i < spin_until; ++i) {
                    _mm_pause(); // Short pause to avoid busy spin
                }
*/
                break;
            }
            const bool samples_now = slot->timestamp(); // For telemetry sampling (every 1024 messages)
            // The message slot remains valid until release_message() is called
            std::string_view sv{ slot->data(), slot->size() };
            // Observability: measure handoff duration (time from transport processing start to protocol consumption)
            WK_TL3(
                std::uint64_t delivery_ns = 0;
                if (samples_now) [[unlikely]] {
                    delivery_ns = clock.now_ns();
                    telemetry_.handoff_latency.record(slot->timestamp(), delivery_ns);
                    slot->reset_timestamp(); // Clear timestamp after recording to prevent stale data in future measurements
                }
            );
            // Handle the message (parsing & routing)
            Method method;
            Channel channel;
            auto r = parser_.parse_and_route(sv, method, channel);
            handle_parser_result_(r, sv);
            // Observability: measure protocol processing duration (time spent inside the protocol layer to process one message)
            WK_TL3(
                if (samples_now) [[unlikely]] {
                    telemetry_.message_process_duration.record(delivery_ns, clock.now_ns());
                }
            );
            // Check to handle parser backpressure if needed
            if (r == parser::Result::Backpressure) {
                WK_WARN("[SESSION] Failed to deliver " << to_string(channel) << " message (backpressure)"
                    " - protocol correctness compromised (user is not draining fast enough)");
                connection_.release_message(slot);
                overload_state_.user.mark_active();
                WK_TL1( telemetry_.user_delivery_failures_total.inc() );
                break; // Stop processing more messages to prevent further damage
            }
            // Release the message slot back to the transport regardless of parsing outcome to maintain flow.
            connection_.release_message(slot);
            WK_TL1( telemetry_.messages_processed_total.inc() );
            ++messages_processed;
        }

        // Observability: track data plane pressure (messages processed per poll)
        WK_TL1(
            if (messages_processed > 0) [[likely]] {
                telemetry_.messages_per_poll.set(messages_processed);
            }
        );

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
        while (ctx_->rejection_ring.pop(notice)) {
            // 1) Apply internal protocol correctness handling
            handle_rejection_(notice);
            // 2) Forward to user-visible rejection buffer (lossless)
            if (!user_rejection_buffer_.push(notice)) [[unlikely]] { // This is a hard failure: we cannot report semantic errors reliably
                WK_WARN("[SESSION] Failed to deliver rejection notice (backpressure) - protocol correctness compromised (user is not draining fast enough)");
                overload_state_.user.mark_active();
                WK_TL1( telemetry_.user_delivery_failures_total.inc() );
                break; // Stop processing more messages to prevent further damage
            }
        }}
        
        // ===============================================================================
        // PROCESS TRADE MESSAGES
        // ===============================================================================
        { // === Process trade subscribe ring ===
        schema::trade::SubscribeAck ack;
        while (ctx_->trade_subscribe_ring.pop(ack)) {
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SESSION] Subscription ACK missing req_id for channel 'trade' {" << ack.symbol << "}");
            }
            else {
                trade_channel_manager_.process_subscribe_ack(ack.req_id.value(), ack.symbol, ack.success);
            }
        }}
        { // === Process trade unsubscribe ring ===
        schema::trade::UnsubscribeAck ack;
        while (ctx_->trade_unsubscribe_ring.pop(ack)) {
            WK_TRACE("[SESSION] Processing trade unsubscribe ACK for symbol {" << ack.symbol << "}");
            //dispatcher_.remove_symbol_handlers<schema::trade::UnsubscribeAck>(ack.symbol);
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SESSION] Unsubscription ACK missing req_id for channel 'trade' {" << ack.symbol << "}");
            }
            else {
                trade_channel_manager_.process_unsubscribe_ack(ack.req_id.value(), ack.symbol, ack.success);
                // Prevent replay of the cancelled symbol after reconnect (only if replay enabled)
                if constexpr (ReplayPolicy::enabled) {
                    if (ack.success) {
                        replay_db_.remove_symbol<schema::trade::Subscribe>(ack.symbol);
                    }
                }
            }
        }}
        // ===============================================================================
        // PROCESS BOOK UPDATES
        // ===============================================================================
        { // === Process book subscribe ring ===
        schema::book::SubscribeAck ack;
        while (ctx_->book_subscribe_ring.pop(ack)) {
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SESSION] Subscription ACK missing req_id for channel 'book' {" << ack.symbol << "}");
            }
            else {
                book_channel_manager_.process_subscribe_ack(ack.req_id.value(), ack.symbol, ack.success);
            }
        }}
        { // === Process book unsubscribe ring ===
        schema::book::UnsubscribeAck ack;
        while (ctx_->book_unsubscribe_ring.pop(ack)) {
            WK_TRACE("[SESSION] Processing book unsubscribe ACK for symbol {" << ack.symbol << "}");
            //dispatcher_.remove_symbol_handlers<schema::book::UnsubscribeAck>(ack.symbol);
            if (!ack.req_id.has()) [[unlikely]] {
                WK_WARN("[SESSION] Unsubscription ACK missing req_id for channel 'book' {" << ack.symbol << "}");
            }
            else {
                book_channel_manager_.process_unsubscribe_ack(ack.req_id.value(), ack.symbol, ack.success);
                // Prevent replay of the cancelled symbol after reconnect (only if replay enabled)
                if constexpr (ReplayPolicy::enabled) {
                    if (ack.success) {
                        replay_db_.remove_symbol<schema::book::Subscribe>(ack.symbol);
                    }
                }
            }
        }}

        // ===============================================================================
        // Batching logic for user requests
        // ===============================================================================
        if constexpr (BatchingPolicy::mode == policy::protocol::BatchingMode::Paced) {
            request_scheduler_.poll();
            if (!request_scheduler_.idle() && request_scheduler_.should_send()) {
                std::string_view msg;
                if (request_scheduler_.peek(msg)) {
                    WK_TRACE("[SESSION] Emitting paced request: " << msg);
                    if (connection_.send(msg)) [[likely]] {
                        WK_TL1( telemetry_.requests_emitted_total.inc() );
                    }
                    else {
                        WK_ERROR("[SESSION] Failed to send paced request - " << msg);
                    }
                    request_scheduler_.release();
                }
            }
        }
        
        // Check for backpressure violations and enforce policy if needed
        enforce_backpressure_policy_();

        return connection_.epoch();
    }

    // Returns true if the current connection is connected
    [[nodiscard]]
    inline bool is_connected() const noexcept {
        return connection_.is_connected();
    }

    // Returns true if the current connection is active
    [[nodiscard]]
    inline bool is_active() const noexcept {
        return connection_.is_active();
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
            ctx_->empty() &&
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

    [[nodiscard]]
    inline telemetry::Session& telemetry() noexcept {
        return telemetry_;
    }

    [[nodiscard]]
    inline lcr::memory::footprint memory_usage() const noexcept {
        lcr::memory::footprint fp;
        fp.add_static(sizeof(*this));
        fp.add(ctx_);
        fp.add_dynamic(connection_);
        return fp;
    }

    inline static void dump_configuration(std::ostream& os) noexcept {
        PolicyBundle::dump(os);
        ConnectionT::dump_configuration(os);
        WS::dump_configuration(os);
    }

#ifdef WK_UNIT_TEST
public:
        inline ConnectionT& connection() {
            return connection_;
        }

        inline WS* ws() {
            return connection_.ws();
        }
#endif // WK_UNIT_TEST

private:
    // Public policy aliases for cleaner access
    using BackpressurePolicy  = typename PolicyBundle::backpressure;
    using LivenessPolicy      = typename PolicyBundle::liveness;
    using SymbolLimitPolicy   = typename PolicyBundle::symbol_limit;
    using ReplayPolicy        = typename PolicyBundle::replay;
    using BatchingPolicy      = typename PolicyBundle::batching;

    // Asserts
    static_assert(BatchingPolicy::batch_size <= MAX_REQUEST_SYMBOLS);

private:
    // Sequence generator for request IDs
    lcr::sequence req_id_seq_{ctrl::PROTOCOL_BASE};

    // Telemetry for the session
    telemetry::Session telemetry_;

    // Underlying connection
    ConnectionT connection_;

    // Temporary buffer for request serialization (to avoid heap allocations)
    lcr::local::raw_buffer<config::protocol::TX_BUFFER_CAPACITY> tx_buffer_{};

    // Request scheduler for huge subscribe requests 
    using RequestSchedulerT =
        std::conditional_t<
            BatchingPolicy::mode == policy::protocol::BatchingMode::Paced,
            protocol::request::Scheduler<BatchingPolicy>,
            protocol::request::NullScheduler
        >;
    RequestSchedulerT request_scheduler_;

    // Session context (owning)
    std::unique_ptr<Context> ctx_;

    // Session context view (non-owning)
    ContextView ctx_view_;

    // Protocol parser / router
    parser::Router parser_;

    // User-visible rejection queue.
    // Decoupled from internal protocol processing to prevent user behavior from affecting Core correctness.
    lcr::local::queue<schema::rejection::Notice, config::protocol::REJECTION_RING_CAPACITY> user_rejection_buffer_;

    // Channel subscription managers
    channel::Manager trade_channel_manager_{Channel::Trade};
    channel::Manager book_channel_manager_{Channel::Book};

    // Replay database
    replay::Database replay_db_;

    // Overall overload state tracking for transport, protocol, and user domains
    struct OverloadState {
        lcr::control::ConsecutiveStateCounter transport;
        lcr::control::FrameConsecutiveStateCounter user;

        inline void next_frame() noexcept {
            transport.next_frame();
            user.next_frame();
        }

        inline void reset() noexcept {
            transport.reset();
            user.reset();
        }
    };
    OverloadState overload_state_;

private:
    
    inline void handle_connect_() {
        WK_TRACE("[SESSION] handle connect (transport_epoch = " << transport_epoch() << ")");
        if constexpr (ReplayPolicy::enabled) {
            WK_DEBUG("[SESSION] Subscription replay is enabled (subscriptions will be re-sent after reconnect)");
            do_replay_();
        }
        else {
            WK_DEBUG("[SESSION] Subscription replay is disabled (no subscriptions will be re-sent after reconnect)");
        }
    }

    inline void do_replay_() noexcept {
        // Replay subscriptions if this is a reconnect (epoch > 1)
        if (transport_epoch() > 1) {
            // 1) Replay trade subscriptions
            auto trade_subscriptions = replay_db_.take_subscriptions<schema::trade::Subscribe>();
            if(!trade_subscriptions.empty()) {
                WK_DEBUG("[REPLAY] Replaying " << trade_subscriptions.size() << " trade subscription(s)");
                for (const auto& subscription : trade_subscriptions) {
                    re_subscribe_(subscription.request());
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
                    re_subscribe_(subscription.request());
                }
            }
            else {
                WK_DEBUG("[REPLAY] No book subscriptions to replay");
            }
        }
    }

    template <request::Subscription RequestT>
    void re_subscribe_(RequestT req) {
        //WK_INFO("[SESSION] Re-subscribing to channel '" << channel_name_of_v<RequestT> << "' " << core::to_string(req.symbols));
        WK_INFO("[SESSION] Re-subscribing to channel '" << channel_name_of_v<RequestT> << "' (total: " << req.symbols.size() << " symbol/s)");
        // 1) Register subscription with manager (internal filtering)
        auto& mgr = subscription_manager_for_<RequestT>();
        auto accepted_symbols = mgr.register_subscription(std::move(req.symbols), req.req_id.value());
        if (accepted_symbols.empty()) {
            WK_TRACE("[SESSION] Re-subscription fully filtered by manager");
            return;
        }
        // 2) Replace request symbols with accepted set
        req.symbols = std::move(accepted_symbols);
        // 3) Register in replay DB using filtered request (only if replay enabled)
        if constexpr (ReplayPolicy::enabled) {
            replay_db_.add(req);
        }
        WK_DEBUG("[SESSION] Emitting re-subscribe message: " << req.symbols.size() << " symbol/s");
        // 4) Emit the request according to the configured batching policy
        if (emit_subscription_(req)) {
            WK_TL1(telemetry_.replay_requests_total.inc());
            WK_TL1(telemetry_.replay_symbols_total.inc(req.symbols.size()) );
        }
    }

    inline void handle_disconnect_() {
        WK_TRACE("[SESSION] handle disconnect (transport_epoch = " << transport_epoch() << ")");
        // Clear runtime state
        trade_channel_manager_.clear_all();
        book_channel_manager_.clear_all();
        overload_state_.reset();
    }

    inline void handle_parser_result_(parser::Result result, std::string_view raw_message) noexcept {
        switch (result) {
        case parser::Result::Ignored:
            WK_TRACE("[SESSION] Message ignored by parser: " << raw_message);
            WK_TL1( telemetry_.parse_ignored_total.inc() );
            break;
        case parser::Result::InvalidJson:
        case parser::Result::InvalidSchema:
        case parser::Result::InvalidValue:
            WK_WARN("[SESSION] Failed to parse message: " << raw_message << " (result: " << to_string(result) << ")");
            WK_TL1( telemetry_.parse_failure_total.inc() );
            break;
        case parser::Result::Parsed:
        case parser::Result::Delivered:
            WK_TL1( telemetry_.parse_success_total.inc() );
            break;
        case parser::Result::Backpressure:
            WK_TL1( telemetry_.parse_backpressure_total.inc() );
            break;
        }
    }

    inline void handle_rejection_(const schema::rejection::Notice& notice) noexcept {
        WK_TL1( telemetry_.rejection_notices_total.inc() );
        WK_WARN("[SESSION] Handling rejection notice for symbol {" << (notice.symbol.has() ? notice.symbol.value() : Symbol("N/A") )
            << "} (req_id=" << (notice.req_id.has() ? notice.req_id.value() : ctrl::INVALID_REQ_ID) << ") - " << notice.error);
        if (notice.req_id.has()) {
            if (notice.symbol.has()) {
                // we cannot infer channel from notice, so we try all managers
                (void)trade_channel_manager_.try_process_rejection(notice.req_id.value(), notice.symbol.value());
                (void)book_channel_manager_.try_process_rejection(notice.req_id.value(), notice.symbol.value());
                // Try process rejection in replay DB to prevent replay of failed subscriptions after reconnect (only if replay enabled)
                if constexpr (ReplayPolicy::enabled) {
                    (void)replay_db_.try_process_rejection(notice.req_id.value(), notice.symbol.value());
                }
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
        case transport::connection::Signal::RetryImmediate:
            // Currently no user-defined hook for immediate retry
            break;
        case transport::connection::Signal::RetryScheduled:
            // Currently no user-defined hook for retry scheduled
            break;
        case transport::connection::Signal::LivenessThreatened:
            if constexpr (LivenessPolicy::proactive) {
                ping();
            }
            break;
        case transport::connection::Signal::BackpressureDetected:
            WK_DEBUG("[SESSION] Transport backpressure detected - protocol correctness compromised (messages are not being processed fast enough)");
            overload_state_.transport.set_active(true);
            break;
        case transport::connection::Signal::BackpressureCleared:
            WK_DEBUG("[SESSION] Transport backpressure cleared - resuming normal processing (consecutive backpressure frames: "
                << overload_state_.transport.count() << " < threshold: " << BackpressurePolicy::escalation_threshold << ")");
            overload_state_.transport.set_active(false);
            break;
        default:
            break;
        }
    }

    inline void enforce_backpressure_policy_() noexcept {
        overload_state_.next_frame();
        if (overload_state_.transport.is_active() && enforce_transport_backpressure_policy_()) [[unlikely]] {
            return; // If transport policy enforcement resulted in connection close, skip user policy to avoid redundant actions
        }
        if (overload_state_.user.is_active() && enforce_user_backpressure_policy_()) [[unlikely]] {
            return; // If user policy enforcement resulted in connection close, skip further actions
        }
    }

    [[nodiscard]]
    inline bool enforce_transport_backpressure_policy_() noexcept {
        using core::policy::BackpressureMode;

        WK_TL1( telemetry_.transport_overload_streak.record(overload_state_.transport.count()) );

        // ZeroTolerance is handled by transport layer (force close on backpressure), so we do not need to do anything here.
        if constexpr (BackpressurePolicy::mode == BackpressureMode::ZeroTolerance) {
            return false;
        }

        if (overload_state_.transport.count() >= BackpressurePolicy::escalation_threshold) [[unlikely]] {
            WK_WARN("[SESSION] Transport backpressure has been active for " << overload_state_.transport.count() << " consecutive polls ("
                << BackpressurePolicy::mode_name() << " backpressure policy)");
            WK_FATAL("[SESSION] Forcing connection close to preserve correctness guarantees");
            connection_.close();
            overload_state_.reset();
            return true;
        }
        return false;
    }

    [[nodiscard]]
    inline bool enforce_user_backpressure_policy_() noexcept {
        using core::policy::BackpressureMode;

        WK_TL1( telemetry_.user_overload_streak.record(overload_state_.user.count()) );

        if constexpr (BackpressurePolicy::mode == BackpressureMode::ZeroTolerance) {
            WK_WARN("[SESSION] User backpressure has been active for " << overload_state_.user.count() << " consecutive polls ("
                << BackpressurePolicy::mode_name() << " backpressure policy)");
            WK_FATAL("[SESSION] Forcing connection close to preserve correctness guarantees");
            connection_.close();
            overload_state_.reset();
            return true;
        }

        if (overload_state_.user.count() >= BackpressurePolicy::escalation_threshold) [[unlikely]] {
            WK_WARN("[SESSION] User backpressure has been active for " << overload_state_.user.count() << " consecutive polls ("
                << BackpressurePolicy::mode_name() << " backpressure policy)");
            WK_FATAL("[SESSION] Forcing connection close to preserve correctness guarantees");
            connection_.close();
            overload_state_.reset();
            return true;
        }
        return false;
    }

    // ------------------------------------------------------------
    // Batching policy enforcement
    // ------------------------------------------------------------
    template <request::Subscription RequestT>
    [[nodiscard]]
    bool emit_subscription_(RequestT req) noexcept {
        if constexpr (BatchingPolicy::mode == policy::protocol::BatchingMode::Immediate) {
            WK_TRACE("[SESSION] Sending immediate subscribe message: " << req.symbols.size() << " symbol/s (req_id=" << req.req_id.value() << ")");
            return send_request_(req);
        }
        else {
            // Batch the subscription request into multiple messages if it exceeds the batch size limit.
            const auto& symbols = req.symbols;
            RequestT batch = req;
            std::size_t total = symbols.size();
            std::size_t offset = 0;
            while (offset < total) {
                batch.symbols.clear();
                const std::size_t n = std::min(BatchingPolicy::batch_size, total - offset);
                for (std::size_t i = 0; i < n; ++i) {
                    batch.symbols.push_back(symbols[offset + i]);
                }
                offset += n;
                if constexpr (BatchingPolicy::mode == policy::protocol::BatchingMode::Batch) {
                    WK_TRACE("[SESSION] Sending batched subscribe message: " << batch.symbols.size() << " symbol/s (req_id=" << batch.req_id.value() << ")");
                    if (!send_request_(batch)) {
                        return false;
                    }
                }
                else {  
                    WK_TRACE("[SESSION] Sending paced subscribe message: " << batch.symbols.size() << " symbol/s (req_id=" << batch.req_id.value() << ")");
                    if (!request_scheduler_.enqueue(batch)) {
                        WK_TL1( telemetry_.request_batching_failures_total.inc() );
                        return false;
                    }
                }
            }
            return true;
        }
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

            if (SymbolLimitPolicy::max_trade > 0 && trade_now + requested > SymbolLimitPolicy::max_trade) {
                WK_WARN("[SESSION:trade] Symbol limit exceeded (" << "active: " << trade_now << ", requested: " << requested << ", max: " << SymbolLimitPolicy::max_trade << ")");
                return false;
            }
        }
        else if constexpr (channel_of_v<RequestT> == Channel::Book) { // Check book limits

            if (SymbolLimitPolicy::max_book > 0 && book_now + requested > SymbolLimitPolicy::max_book) {
                WK_WARN("[SESSION:book] Symbol limit exceeded (" << "active: " << book_now << ", requested: " << requested << ", max: " << SymbolLimitPolicy::max_book << ")");
                return false;
            }
        }
        // Check global limits
        if (SymbolLimitPolicy::max_global > 0 && global_now + requested > SymbolLimitPolicy::max_global) {
            WK_WARN("[SESSION] Global symbol limit exceeded (" << "active: " << global_now << ", requested: " << requested << ", max: " << SymbolLimitPolicy::max_global << ")");
            return false;
        }
        return true;
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


    // -----------------------------------------------------------------------------
    // Send request (unified entry point)
    // -----------------------------------------------------------------------------
    // Unified send_request_ that dispatches to the appropriate implementation
    // based on the request type's JSON writability category.
    // - One API
    // - Two implementations
    // - Compile-time dispatch
    // - Zero runtime overhead
    // -----------------------------------------------------------------------------

    template <typename RequestT>
    requires StaticJsonWritable<RequestT> && request::ValidRequestIntent<RequestT>
    [[nodiscard]]
    inline bool send_request_(RequestT req) noexcept {
        return send_static_request_(std::move(req));
    }

    template <typename RequestT>
    requires DynamicJsonWritable<RequestT> && request::ValidRequestIntent<RequestT>
    [[nodiscard]]
    inline bool send_request_(RequestT req) noexcept {
        return send_dynamic_request_(std::move(req));
    }

    // -----------------------------------------------------------------------------
    // Send request with compile-time bounded JSON size
    // -----------------------------------------------------------------------------
    template <typename RequestT>
    requires StaticJsonWritable<RequestT> && request::ValidRequestIntent<RequestT>
    [[nodiscard]]
    inline bool send_static_request_(RequestT req) noexcept {
        // 0. Static assertions to ensure correct usage of this helper
        static_assert(request::ValidRequestIntent<RequestT>, "Invalid request type: must define exactly one intent tag");
        // 1. Assign req_id if missing
        if (!req.req_id.has()) {
            req.req_id = req_id_seq_.next();
        }
        // 2. Serialize to JSON (into stack buffer to avoid heap allocation)
        char buffer[RequestT::max_json_size()];
        const std::size_t size = req.write_json(buffer);
        LCR_ASSERT_MSG(size <= RequestT::max_json_size(), "Serialized JSON size exceeds static buffer capacity");
        // 3. Send the request
        std::string_view msg{buffer, size};
        WK_TRACE("[SESSION] Sending json request (payload: " << lcr::format_bytes_exact(size) << ")");
        if (!connection_.send(msg)) {
            WK_ERROR("[SESSION] Failed to send request (req_id=" << lcr::to_string(req.req_id) << ") - " << msg);
            return false;
        }
        WK_TL1( telemetry_.requests_emitted_total.inc() );
        return true;
    }


    // -----------------------------------------------------------------------------
    // Send request with runtime-computed JSON size
    // -----------------------------------------------------------------------------
    template <typename RequestT>
    requires DynamicJsonWritable<RequestT> && request::ValidRequestIntent<RequestT>
    [[nodiscard]]
    inline bool send_dynamic_request_(RequestT req) noexcept {
        // 0. Static assertions to ensure correct usage of this helper
        static_assert(request::ValidRequestIntent<RequestT>, "Invalid request type: must define exactly one intent tag");
        // 1. Assign req_id if missing
        if (!req.req_id.has()) {
            req.req_id = req_id_seq_.next();
        }
        // 2. Serialize to JSON (into stack buffer to avoid heap allocation)
        const std::size_t required = req.max_json_size();
        if (required > config::protocol::TX_BUFFER_CAPACITY) [[unlikely]] {
            WK_FATAL("[SESSION] JSON exceeds TX buffer capacity");  // TODO: handle error more gracefully (bad result on examples with large symbol lists & buffer overflow)
            return false;
        }
        tx_buffer_.reset();
        const std::size_t size = req.write_json(tx_buffer_.data());
        tx_buffer_.set_size(size);
        LCR_ASSERT_MSG(size <= required, "Serialized JSON size exceeds static buffer capacity");
        // 3. Send the request
        std::string_view msg{tx_buffer_.data(), tx_buffer_.size()};
        WK_TRACE("[SESSION] Sending json request (payload: " << lcr::format_bytes_exact(size) << ")");
        if (!connection_.send(msg)) {
            WK_ERROR("[SESSION] Failed to send request (req_id=" << lcr::to_string(req.req_id) << ") - " << msg);
            return false;
        }
        WK_TL1( telemetry_.requests_emitted_total.inc() );
        return true;
    }

};

} // namespace wirekrak::core::protocol::kraken

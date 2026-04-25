
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
#include "wirekrak/core/protocol/message_result.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"
#include "wirekrak/core/protocol/subscription/controller.hpp"
#include "wirekrak/core/protocol/replay/database.hpp"
#include "wirekrak/core/protocol/request/concepts.hpp"
#include "wirekrak/core/protocol/request/scheduler.hpp"
#include "wirekrak/core/protocol/telemetry/session.hpp"
#include "wirekrak/core/protocol/kraken/subscriptions/traits.hpp"
#include "wirekrak/core/protocol/kraken/subscriptions/set.hpp"
#include "wirekrak/core/protocol/kraken/context.hpp"
#include "wirekrak/core/protocol/kraken/schema/system/ping.hpp"
#include "wirekrak/core/protocol/kraken/channel_traits.hpp"
#include "wirekrak/core/protocol/kraken/parser/router.hpp"
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

    template<class T>
    using domain_t = subscription_type<T>;

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
        WK_DEBUG("[SESSION] Closing connection...");
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
        auto accepted_symbols = subscription_controller_.template
            register_subscription<domain_t<RequestT>>(std::move(req.symbols), req.req_id.value());
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
            replay_db_.add<domain_t<RequestT>>(req);
        }
        // 6) Emit the request according to the configured batching policy
        WK_DEBUG("[SESSION] Emitting subscribe message: " << req.symbols.size() << " symbol/s (req_id=" << req.req_id.value() << ")");
        if (!emit_request_(req)) {
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
        WK_DEBUG("[SESSION] Emitting unsubscribe message: " << req.symbols.size() << " symbol/s (req_id=" << req.req_id.value() << ")");
        if (!emit_request_(req)) {
            return ctrl::INVALID_REQ_ID;
        }
        WK_TL1(telemetry_.unsubscriptions_requested_total.inc());
        // 3) Tell subscription manager we are awaiting an ACK (transfer ownership of symbols)
        RequestSymbols cancelled = subscription_controller_.template
            register_unsubscription<domain_t<RequestT>>(std::move(req.symbols), req.req_id.value());
        // 4) Update replay DB to prevent replay of the cancelled symbols after reconnect (only if replay enabled)
        if constexpr (ReplayPolicy::enabled) {
            for (const auto& symbol : cancelled) {
                replay_db_.remove_symbol<domain_t<RequestT>>(symbol);
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
        WK_TL3(
            auto now = clock.now_ns();
            if (last_poll_ns_ != 0) {
                const auto delta = now - last_poll_ns_;
                if (!overload_state_.transport.is_active()) [[likely]] {
                    telemetry_.healthy_time_ns.inc(delta);
                }
                else {
                    telemetry_.backpressure_time_ns.inc(delta);
                }
            }
            last_poll_ns_ = now;
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
            const auto slot_create_ts_ns = slot->create_ts(); // Capture create timestamp before processing for accurate latency measurement
            const bool samples_now = slot_create_ts_ns; // For telemetry sampling (every 1024 messages)
            // The message slot remains valid until release_message() is called
            std::string_view sv{ slot->data(), slot->size() };
            // Observability: measure handoff duration (time from transport processing start to protocol consumption)
            WK_TL3(
                std::uint64_t delivery_ns = 0;
                if (samples_now) [[unlikely]] {
                    delivery_ns = clock.now_ns();
                    telemetry_.handoff_latency.record(slot->delivery_ts(), delivery_ns);
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
                    telemetry_.process_latency.record(delivery_ns, clock.now_ns());
                }
            );
            // Check to handle parser backpressure if needed
            if (r == MessageResult::Backpressure) {
                WK_WARN("[SESSION] Failed to deliver " << to_string(channel) << " message (backpressure)"
                    " - protocol correctness compromised (user is not draining fast enough)");
                connection_.release_message(slot);
                //overload_state_.user.mark_active(); <-- TODO: Fast fail here
                WK_TL1( telemetry_.user_delivery_failures_total.inc() );
                break; // Stop processing more messages to prevent further damage
            }
            // Release the message slot back to the transport regardless of parsing outcome to maintain flow.
            connection_.release_message(slot);
            // Observability: measure end to end latency up to final user delivery (including protocol processing)
            WK_TL3(
                if (samples_now) [[unlikely]] {
                    telemetry_.end_to_end_latency.record(slot_create_ts_ns, clock.now_ns());
                }
            );
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
                //overload_state_.user.mark_active(); <-- TODO: Fast fail here
                WK_TL1( telemetry_.user_delivery_failures_total.inc() );
                break; // Stop processing more messages to prevent further damage
            }
        }}
        
        // ===============================================================================
        // PROCESS TRADE MESSAGES
        // ===============================================================================
        process_subscribe_ack_ring_<schema::trade::SubscribeAck>(ctx_->trade_subscribe_ring);

        process_unsubscribe_ack_ring_<schema::trade::UnsubscribeAck>(ctx_->trade_unsubscribe_ring);
        
        // ===============================================================================
        // PROCESS BOOK UPDATES
        // ===============================================================================
        process_subscribe_ack_ring_<schema::book::SubscribeAck>(ctx_->book_subscribe_ring);

        process_unsubscribe_ack_ring_<schema::book::UnsubscribeAck>(ctx_->book_unsubscribe_ring);

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

    template<class AckT, class RingT>
    void process_subscribe_ack_ring_(RingT& ring) {
        AckT ack;
        while (ring.pop(ack)) {
            if (ack.req_id.has()) [[likely]] {
                subscription_controller_.template
                    process_subscribe_ack<domain_t<AckT>>(ack.req_id.value(), ack.symbol, ack.success);
            }
            else {
                // TODO: Increment a metric for ACKs with missing req_id to monitor potential protocol issues
            }
        }
    }

    template<class AckT, class RingT>
    void process_unsubscribe_ack_ring_(RingT& ring) {
        AckT ack;
        while (ring.pop(ack)) {
            if (ack.req_id.has()) [[likely]] {
                subscription_controller_.template
                    process_unsubscribe_ack<domain_t<AckT>>(ack.req_id.value(), ack.symbol, ack.success);
                // Prevent replay of the cancelled symbol after reconnect (only if replay enabled)
                if constexpr (ReplayPolicy::enabled) {
                    if (ack.success) {
                        replay_db_.remove_symbol<domain_t<AckT>>(ack.symbol);
                    }
                }
            }
            else {
                // TODO: Increment a metric for ACKs with missing req_id to monitor potential protocol issues
            }
        }
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
    inline const auto& trade_subscriptions() const noexcept {
        return subscription_manager_for_<schema::trade::Subscribe>();
    }

    // Accessor to the book subscription manager
    [[nodiscard]]
    inline const auto& book_subscriptions() const noexcept {
        return subscription_manager_for_<schema::book::Subscribe>();
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
    // Protocol quiescence indicator (strict, deterministic)
    // -----------------------------------------------------------------------------
    //
    // Returns true if the Session is **protocol-quiescent**.
    //
    // Quiescent means that, at the current instant:
    //
    //   • No subscribe or unsubscribe requests are awaiting ACKs
    //   • No replay or reconnect-driven protocol work is pending
    //   • No internal protocol state requires further poll() calls
    //   • Transport has no pending control-plane work
    //   • No rejection messages remain undrained
    //
    // In other words:
    //
    //   If poll() is never called again, the Session will not violate
    //   protocol correctness or leave the exchange in an inconsistent state.
    //
    // IMPORTANT SEMANTICS:
    //
    // • This is a STRICT, deterministic signal (no time-based heuristics)
    //
    // • This is NOT a data-plane signal:
    //     Active subscriptions may still produce future data
    //
    // • This does NOT guarantee that all user-visible messages
    //     (trade, book, status, pong) have been drained
    //
    // • This does NOT imply that the transport is closed or inactive
    //
    // Threading & usage:
    //
    // • Not thread-safe
    // • Intended to be queried from the Session event loop
    // • Used for correctness-sensitive logic
    //
    // -----------------------------------------------------------------------------
    [[nodiscard]]
    inline bool is_quiescent() const noexcept {
        return
            connection_.is_idle() &&          // transport has no pending work
            ctx_->empty() &&                 // no pending parsed artifacts
            user_rejection_buffer_.empty() &&// no undrained semantic errors
            subscription_controller_.is_quiescent(); // STRICT (no timeout)
    }

    // -----------------------------------------------------------------------------
    // Protocol idle indicator (graceful shutdown heuristic)
    // -----------------------------------------------------------------------------
    //
    // Returns true if the Session is **operationally idle**.
    //
    // Idle means:
    //
    //   • The Session is either strictly quiescent
    //     OR
    //   • No protocol progress has been observed for a configured timeout
    //
    // This is a TIME-BASED heuristic built on top of protocol progress tracking.
    // It allows the system to stop waiting for late or lost ACKs during shutdown.
    //
    // IMPORTANT SEMANTICS:
    //
    // • This may return true even if:
    //     - Some subscriptions are still awaiting ACK
    //     - The exchange has not completed all protocol flows
    //
    // • This is INTENDED for:
    //     - Graceful shutdown loops
    //     - Drain-with-timeout patterns
    //
    // • This MUST NOT be used for:
    //     - Protocol correctness decisions
    //     - State validation or invariants
    //
    // • When ProgressPolicy::enabled == false:
    //     - This degenerates to strict quiescence (never lies)
    //
    // Threading & usage:
    //
    // • Not thread-safe
    // • Intended for outer control loops (shutdown, lifecycle management)
    //
    // -----------------------------------------------------------------------------
    // Example:
    //
    //   // Graceful shutdown with timeout fallback
    //   while (!session.is_idle()) {
    //       session.poll();
    //       drain_messages();
    //   }
    //
    // -----------------------------------------------------------------------------
    [[nodiscard]]
    inline bool is_idle() const noexcept {
        // Fast path: strict quiescence
        if (is_quiescent()) {
            return true;
        }
        // If no timeout policy → never lie
        if constexpr (!ProgressPolicy::enabled) {
            return false;
        }
        // Timeout-based fallback (controller already tracks progress globally)
        return subscription_controller_.is_idle();
    }

    // -----------------------------------------------------------------------------
    // Protocol stall indicator
    // -----------------------------------------------------------------------------
    //
    // Returns true if the Session is **stalled**.
    //
    // A stalled state occurs when:
    //   • The protocol has NOT reached quiescence (pending work still exists)
    //   • The progress timeout has been exceeded (no forward progress observed)
    //
    // In other words:
    //   The system is waiting for external events (e.g. ACKs, rejections),
    //   but none have been observed within the configured timeout window.
    //
    // This typically indicates:
    //   • Exchange-side delays or dropped messages
    //   • Partial or missing ACKs
    //   • Protocol-level convergence failure
    //
    // IMPORTANT SEMANTICS:
    //
    // • This is a **diagnostic signal**, not a correctness guarantee.
    //   The Session may still be in a logically inconsistent state.
    //
    // • This does NOT imply that the transport is disconnected or unhealthy.
    //
    // • This does NOT modify internal state or force convergence.
    //
    // • Behavior depends on ProgressPolicy:
    //     - Strict: always false (no timeout fallback)
    //     - Timeout: becomes true after timeout_ns without progress
    //
    // Threading & usage:
    //   • Not thread-safe
    //   • Intended for observability, logging, and shutdown decisions
    //
    // Typical usage:
    //
    //   if (session.is_stalled()) {
    //       log_timeout();
    //       // optional: force close or escalate
    //   }
    //
    // -----------------------------------------------------------------------------
    [[nodiscard]]
    inline bool is_stalled() const noexcept {
        return !is_quiescent() && is_idle();
    }

    [[nodiscard]]
    inline std::size_t pending_protocol_requests() const noexcept {
        return subscription_controller_.pending_requests();
    }

    [[nodiscard]]
    inline std::size_t pending_protocol_symbols() const noexcept {
        return subscription_controller_.pending_symbols();
    }

    [[nodiscard]]
    inline const auto& subscription_controller() const noexcept {
        return subscription_controller_;
    }

    [[nodiscard]]
    inline const auto& replay_database() const noexcept {
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
    using ProgressPolicy      = typename PolicyBundle::progress;
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

    // Timestamps for observability
    std::uint64_t last_poll_ns_;

    // Underlying connection
    ConnectionT connection_;

    // Temporary buffer for request serialization (to avoid heap allocations)
    lcr::local::raw_buffer<config::protocol::TX_BUFFER_CAPACITY> tx_buffer_{};

    // Request scheduler for huge subscribe requests 
    using RequestSchedulerT =
        std::conditional_t<
            BatchingPolicy::mode == policy::protocol::BatchingMode::Paced,
            request::Scheduler<BatchingPolicy>,
            request::NullScheduler
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

    using SubscriptionController = subscription::Controller<
        ProgressPolicy,
        kraken::SubscriptionSet::types
    >;
    SubscriptionController subscription_controller_;

    // Replay database
    using ReplayDB = replay::Database<
        kraken::SubscriptionSet::types
    >;
    ReplayDB replay_db_;

    // Overall overload state tracking for transport, protocol, and user domains
    struct OverloadState {
        lcr::control::ConsecutiveStateCounter transport;

        inline void next_frame() noexcept {
            transport.next_frame();
        }

        inline void reset() noexcept {
            transport.reset();
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
            replay_db_.for_each([&]<class T>() {
            auto subs = replay_db_.take_subscriptions<T>();
            if (!subs.empty()) {
                WK_DEBUG("[REPLAY] Replaying " << subs.size() << " " << channel_name_of_v<T> << " subscription(s)");
                for (const auto& sub : subs) {
                    re_subscribe_(sub.request());
                }
            }
            else {
                WK_DEBUG("[REPLAY] No " << channel_name_of_v<T> << " subscriptions to replay");
            }
        });
        }
    }

    template <request::Subscription RequestT>
    void re_subscribe_(RequestT req) {
        //WK_INFO("[SESSION] Re-subscribing to channel '" << channel_name_of_v<RequestT> << "' " << core::to_string(req.symbols));
        WK_INFO("[SESSION] Re-subscribing to channel '" << channel_name_of_v<RequestT> << "' (total: " << req.symbols.size() << " symbol/s)");
        // 1) Register subscription with manager (internal filtering)
        auto accepted_symbols = subscription_controller_.template
            register_subscription<domain_t<RequestT>>(std::move(req.symbols), req.req_id.value());
        if (accepted_symbols.empty()) {
            WK_TRACE("[SESSION] Re-subscription fully filtered by manager");
            return;
        }
        // 2) Replace request symbols with accepted set
        req.symbols = std::move(accepted_symbols);
        // 3) Register in replay DB using filtered request (only if replay enabled)
        if constexpr (ReplayPolicy::enabled) {
            replay_db_.add<domain_t<RequestT>>(req);
        }
        WK_DEBUG("[SESSION] Emitting re-subscribe message: " << req.symbols.size() << " symbol/s");
        // 4) Emit the request according to the configured batching policy
        if (emit_request_(req)) {
            WK_TL1(telemetry_.replay_requests_total.inc());
            WK_TL1(telemetry_.replay_symbols_total.inc(req.symbols.size()) );
        }
    }

    inline void handle_disconnect_() {
        WK_TRACE("[SESSION] handle disconnect (transport_epoch = " << transport_epoch() << ")");
        // Clear runtime state
        subscription_controller_.clear_all();
        overload_state_.reset();
    }

    inline void handle_parser_result_(MessageResult result, std::string_view raw_message) noexcept {
        switch (result) {
        case MessageResult::Ignored:
            WK_TRACE("[SESSION] Message ignored by parser: " << raw_message);
            WK_TL1( telemetry_.parse_ignored_total.inc() );
            break;
        case MessageResult::InvalidJson:
        case MessageResult::InvalidSchema:
        case MessageResult::InvalidValue:
            WK_WARN("[SESSION] Failed to parse message: " << raw_message << " (result: " << to_string(result) << ")");
            WK_TL1( telemetry_.parse_failure_total.inc() );
            break;
        case MessageResult::Parsed:
        case MessageResult::Delivered:
            WK_TL1( telemetry_.parse_success_total.inc() );
            break;
        case MessageResult::Backpressure:
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
                (void)subscription_controller_.try_process_rejection(notice.req_id.value(), notice.symbol.value());
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

    // ------------------------------------------------------------
    // Batching policy enforcement
    // ------------------------------------------------------------
    template <typename RequestT>
    [[nodiscard]]
    bool emit_request_(RequestT req) noexcept {
        if constexpr (BatchingPolicy::mode == policy::protocol::BatchingMode::Immediate) {
            WK_TRACE("[SESSION] Sending immediate request: " << req.symbols.size() << " symbol/s (req_id=" << req.req_id.value() << ")");
            return send_request_(req);
        }
        else {
            // Batch the request into multiple messages if it exceeds the batch size limit.
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
                    WK_TRACE("[SESSION] Sending batched request: " << batch.symbols.size() << " symbol/s (req_id=" << batch.req_id.value() << ")");
                    if (!send_request_(batch)) {
                        return false;
                    }
                }
                else {  
                    WK_TRACE("[SESSION] Sending paced request: " << batch.symbols.size() << " symbol/s (req_id=" << batch.req_id.value() << ")");
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
        // --------------------------------------------------------
        // Per-request limit
        // --------------------------------------------------------
        if constexpr (SymbolLimitPolicy::max_per_channel > 0) {
            const std::size_t current = subscription_controller_.template manager_for<domain_t<RequestT>>().total_symbols();
            if (current + requested > SymbolLimitPolicy::max_per_channel) {
                WK_WARN("[SESSION] Symbol limit exceeded (" << "active: " << current
                    << ", requested: " << requested << ", max: " << SymbolLimitPolicy::max_per_channel << ")");
                return false;
            }
        }
        // --------------------------------------------------------
        // Global limit
        // --------------------------------------------------------
        if constexpr (SymbolLimitPolicy::max_global > 0) {
            const std::size_t current = subscription_controller_.total_symbols();
            if (current + requested > SymbolLimitPolicy::max_global) {
                WK_WARN("[SESSION] Symbol limit exceeded (global) " << "(active: " << current
                    << ", requested: " << requested << ", max: " << SymbolLimitPolicy::max_global << ")");
                return false;
            }
        }

        return true;
    }

    // ------------------------------------------------------------
    // Const helper to get the correct subscription manager
    // ------------------------------------------------------------
    template<class MessageT>
    const auto& subscription_manager_for_() const {
        return subscription_controller_.template manager_for<MessageT>();
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

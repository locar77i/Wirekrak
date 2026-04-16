#pragma once

#include <tuple>
#include <utility>
#include <type_traits>

#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/protocol/subscription/manager.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"
#include "wirekrak/core/protocol/subscription_traits.hpp"

namespace wirekrak::core::protocol::subscription {

// ============================================================================
// Subscription Controller (Generic, Type-Indexed)
// ============================================================================
//
// The Subscription Controller manages **subscription lifecycle state** across multiple
// request types using compile-time dispatch.
//
// It replaces per-subscription members like:
//     trade_subscription_manager_
//     book_subscription_manager_
//
// with a single type-indexed container:
//
//     Controller<TradeSubscribe, BookSubscribe>
//
// ---------------------------------------------------------------------------
// Design goals
// ---------------------------------------------------------------------------
// • Fully generic (exchange-agnostic)
// • Zero runtime overhead (compile-time dispatch via std::tuple)
// • No virtual functions, no dynamic allocation
// • Extensible via RequestT type list
//
// ---------------------------------------------------------------------------
// Responsibilities
// ---------------------------------------------------------------------------
// • Track active + pending subscriptions
// • Register subscriptions / unsubscriptions
// • Process ACKs (subscribe / unsubscribe)
// • Process rejections (cross-subscription)
// • Provide aggregate diagnostics
//
// ---------------------------------------------------------------------------
// Usage
// ---------------------------------------------------------------------------
//
// using Channels = Controller<
//     schema::trade::Subscribe,
//     schema::book::Subscribe
// >;
//
// Channels channels;
//
// auto accepted = channels.register_subscription(req);
// auto cancelled = channels.register_unsubscription(req);
//
// channels.process_subscribe_ack<schema::trade::Subscribe>(req_id, symbol, success);
//
// channels.try_process_rejection(req_id, symbol);
//
// ---------------------------------------------------------------------------
// Notes
// ---------------------------------------------------------------------------
// • RequestT is expected to be the *Subscribe* request type
// • Unsubscribe flows map to the same RequestT via subscription_traits
// • Manager<RequestT> must expose:
//      - register_subscription
//      - register_unsubscription
//      - process_subscribe_ack
//      - process_unsubscribe_ack
//      - try_process_rejection
//      - clear_all
//      - pending_requests / total_symbols
//
// ============================================================================

template<class... SubscriptionTs>
class Controller {
public:
    Controller() = default;
    ~Controller() = default;

    // ------------------------------------------------------------------------
    // Subscription registration
    // ------------------------------------------------------------------------
    template<class RequestT>
    inline auto register_subscription(RequestT& req) noexcept {
        return manager_<subscription_type<RequestT>>().register_subscription(
            std::move(req.symbols),
            req.req_id.value()
        );
    }

    // ------------------------------------------------------------------------
    // Unsubscription registration
    // ------------------------------------------------------------------------
    template<class RequestT>
    inline auto register_unsubscription(RequestT& req) noexcept {
        return manager_<subscription_type<RequestT>>().register_unsubscription(
            std::move(req.symbols),
            req.req_id.value()
        );
    }

    // ------------------------------------------------------------------------
    // ACK handling
    // ------------------------------------------------------------------------
    template<class RequestT>
    inline void process_subscribe_ack(ctrl::req_id_t req_id, Symbol symbol, bool success) noexcept {
        manager_<subscription_type<RequestT>>().process_subscribe_ack(req_id, symbol, success);
    }

    template<class RequestT>
    inline void process_unsubscribe_ack(ctrl::req_id_t req_id, Symbol symbol, bool success) noexcept {
        manager_<subscription_type<RequestT>>().process_unsubscribe_ack(req_id, symbol, success);
    }

    // ------------------------------------------------------------------------
    // Rejection handling (cross-subscription)
    // ------------------------------------------------------------------------
    [[nodiscard]]
    inline bool try_process_rejection(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        bool done = false;
        std::apply([&](auto&... mgr) {
            ((done = done || mgr.try_process_rejection(req_id, symbol)), ...);
        }, managers_);

        return done;
    }

    // ------------------------------------------------------------------------
    // Clear all state
    // ------------------------------------------------------------------------
    inline void clear_all() noexcept {
        std::apply([](auto&... mgr) {
            (mgr.clear_all(), ...);
        }, managers_);
    }

    // ------------------------------------------------------------------------
    // Diagnostics
    // ------------------------------------------------------------------------
    [[nodiscard]]
    inline bool is_idle() const noexcept {
        bool idle = true;
        std::apply([&](const auto&... mgr) {
            ((idle = idle && !mgr.has_pending_requests()), ...);
        }, managers_);
        return idle;
    }

    [[nodiscard]]
    inline std::size_t pending_requests() const noexcept {
        std::size_t total = 0;
        std::apply([&](const auto&... mgr) {
            ((total += mgr.pending_requests()), ...);
        }, managers_);
        return total;
    }

    [[nodiscard]]
    std::size_t pending_symbols() const noexcept {
        std::size_t total = 0;
        std::apply([&](const auto&... mgr) {
            ((total += mgr.pending_symbols()), ...);
        }, managers_);
        return total;
    }

    [[nodiscard]]
    inline std::size_t total_symbols() const noexcept {
        std::size_t total = 0;
        std::apply([&](const auto&... mgr) {
            ((total += mgr.total_symbols()), ...);
        }, managers_);
        return total;
    }

    // ------------------------------------------------------------
    // Access the Manager associated with a given RequestT
    // (resolved via subscription_traits)
    // ------------------------------------------------------------
    template<class RequestT>
    auto& manager_for() noexcept {
        return manager_<subscription_type<RequestT>>();
    }

    template<class RequestT>
    const auto& manager_for() const noexcept {
        return manager_<subscription_type<RequestT>>();
    }

    // ------------------------------------------------------------
    // Iterate over all subscription types
    // ------------------------------------------------------------
    //
    // Invokes a callable once per subscription type (SubscriptionT)
    //
    // The callable must support a templated call operator:
    //
    //   db.for_each([&]<class T>() {
    //       // use T
    //   });
    //
    // This enables fully generic operations without
    // exposing the internal torage or requiring runtime dispatch.
    //
    // ------------------------------------------------------------

    template<class F>
    void for_each(F&& f) noexcept {
        (f.template operator()<SubscriptionTs>(), ...);
    }

    template<class F>
    void for_each(F&& f) const noexcept {
        (f.template operator()<SubscriptionTs>(), ...);
    }

    // ------------------------------------------------------------
    // Iterate over all subscription managers
    // ------------------------------------------------------------
    //
    // Similar to for_each, but provides direct access to each Manager<SubscriptionT>.
    //
    //   db.for_each_manager([&]<class T>(Manager<T>& mgr) {
    //       // use T and mgr
    //   });
    //
    // This is useful for operations that need to inspect or modify the managers directly.
    //
    // ------------------------------------------------------------

    template<class F>
    void for_each_manager(F&& f) noexcept {
        (f.template operator()<SubscriptionTs>(manager_<SubscriptionTs>()), ...);
    }

    template<class F>
    void for_each_manager(F&& f) const noexcept {
        (f.template operator()<SubscriptionTs>(manager_<SubscriptionTs>()), ...);
    }

private:
    // ------------------------------------------------------------------------
    // Storage: one Manager per subscription type
    // ------------------------------------------------------------------------
    std::tuple<Manager<SubscriptionTs>...> managers_;

    // ------------------------------------------------------------------------
    // Access helpers
    // ------------------------------------------------------------------------
    template<class RequestT>
    inline auto& manager_() noexcept {
        static_assert((std::is_same_v<RequestT, SubscriptionTs> || ...), "Controller does not manage this subscription type");
        return std::get<Manager<RequestT>>(managers_);
    }

    template<class RequestT>
    inline const auto& manager_() const noexcept {
        static_assert((std::is_same_v<RequestT, SubscriptionTs> || ...), "Controller does not manage this subscription type");
        return std::get<Manager<RequestT>>(managers_);
    }
};

} // namespace wirekrak::core::protocol::subscription

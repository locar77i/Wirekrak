#pragma once

/*
===============================================================================
Subscription Controller (Type-Indexed, Progress-Aware)
===============================================================================

Purpose
-------
Coordinates multiple subscription Managers and provides:

    • Cross-subscription aggregation
    • Progress tracking
    • Timeout-based completion fallback

This is the layer that bridges:

    Manager  → symbol-level lifecycle (strict, no timing)
    Session  → protocol lifecycle & shutdown semantics

-------------------------------------------------------------------------------
Architecture
-------------------------------------------------------------------------------

    Controller<ProgressPolicy, TradeSubscribe, BookSubscribe>

        ├── Manager<TradeSubscribe>
        ├── Manager<BookSubscribe>
        └── Progress tracking (timestamp-based)

All dispatch is resolved at compile-time (std::tuple).

-------------------------------------------------------------------------------
Core Responsibilities
-------------------------------------------------------------------------------

• Forward operations to the correct Manager:
    - register_subscription
    - register_unsubscription
    - process ACKs
    - process rejections

• Aggregate state across all subscription types

• Track protocol progress:
    - subscription registration
    - ACK processing
    - rejection handling
    - state reset

• Provide completion signals:
    - is_quiescent() → strict (no pending work)
    - is_idle()      → timeout-based fallback

-------------------------------------------------------------------------------
Semantics
-------------------------------------------------------------------------------

Quiescent (strict)
    • No pending requests in any Manager
    • Deterministic and exact

Idle (timeout-based)
    • Either quiescent OR
    • No observable progress for ProgressPolicy::timeout_ns

This enables bounded shutdown even when exchanges:
    • drop ACKs
    • partially acknowledge requests
    • violate protocol guarantees

-------------------------------------------------------------------------------
Design Goals
-------------------------------------------------------------------------------

• Fully generic (exchange-agnostic)
• Zero runtime polymorphism
• Compile-time dispatch (no virtuals)
• No dynamic allocation
• Policy-driven behavior (ProgressPolicy)

-------------------------------------------------------------------------------
Progress Model
-------------------------------------------------------------------------------

Progress is recorded when:

    • A subscription is accepted (non-empty registration)
    • An unsubscription modifies state
    • A valid ACK is processed
    • A rejection is processed
    • State is cleared

The Controller does NOT infer progress:
    → it only reacts to externally observable events

-------------------------------------------------------------------------------
Notes
-------------------------------------------------------------------------------

• RequestT is expected to be the Subscribe type
• Unsubscribe flows map via subscription_traits
• Manager<T> owns all symbol-level correctness
• Controller never inspects symbol state directly

===============================================================================
*/

#include <tuple>
#include <utility>
#include <type_traits>

#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/protocol/subscription/manager.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"
#include "wirekrak/core/protocol/subscription_traits.hpp"
#include "lcr/system/monotonic_clock.hpp"


namespace wirekrak::core::protocol::subscription {

template<class ProgressPolicy, class... SubscriptionTs>
class Controller {
public:
    Controller() = default;
    ~Controller() = default;

    // ------------------------------------------------------------------------
    // Subscription registration
    // ------------------------------------------------------------------------
    template<class RequestT>
    auto register_subscription(RequestSymbols symbols, ctrl::req_id_t req_id) noexcept {
        mark_progress_();
        return manager_<subscription_type<RequestT>>().register_subscription(std::move(symbols), req_id);
    }

    // ------------------------------------------------------------------------
    // Unsubscription registration
    // ------------------------------------------------------------------------
    template<class RequestT>
    auto register_unsubscription(RequestSymbols symbols, ctrl::req_id_t req_id) noexcept {
        mark_progress_();
        return manager_<subscription_type<RequestT>>().register_unsubscription(std::move(symbols), req_id);
    }

    // ------------------------------------------------------------------------
    // ACK handling
    // ------------------------------------------------------------------------
    template<class RequestT>
    void process_subscribe_ack(ctrl::req_id_t req_id, Symbol symbol, bool success) noexcept {
        if (manager_<subscription_type<RequestT>>().process_subscribe_ack(req_id, symbol, success)) {
            mark_progress_();
        }
    }

    template<class RequestT>
    void process_unsubscribe_ack(ctrl::req_id_t req_id, Symbol symbol, bool success) noexcept {
        if (manager_<subscription_type<RequestT>>().process_unsubscribe_ack(req_id, symbol, success)) {
            mark_progress_();
        }
    }

    // ------------------------------------------------------------------------
    // Rejection handling (cross-subscription)
    // ------------------------------------------------------------------------
    [[nodiscard]]
    bool try_process_rejection(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        bool done = false;
        std::apply([&](auto&... mgr) {
            ((done = done || mgr.try_process_rejection(req_id, symbol)), ...);
        }, managers_);
        if (done) {
            mark_progress_();
        }
        return done;
    }

    // ------------------------------------------------------------------------
    // Clear all state
    // ------------------------------------------------------------------------
    void clear_all() noexcept {
        std::apply([](auto&... mgr) {
            (mgr.clear_all(), ...);
        }, managers_);
        mark_progress_(); // reset baseline
    }

    // ------------------------------------------------------------------------
    // Diagnostics
    // ------------------------------------------------------------------------
    [[nodiscard]]
    bool is_quiescent() const noexcept {
        bool idle = true;
        std::apply([&](const auto&... mgr) {
            ((idle = idle && !mgr.has_pending_requests()), ...);
        }, managers_);
        return idle;
    }

    [[nodiscard]]
    bool is_idle() const noexcept {
        if (is_quiescent()) {
            return true;
        }
        if constexpr (!ProgressPolicy::enabled) {
            return false;
        }
        const auto now = lcr::system::monotonic_clock::instance().now_ns();
        return (now - last_progress_ns_) > ProgressPolicy::timeout_ns;
    }
    

    [[nodiscard]]
    std::size_t pending_requests() const noexcept {
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
    std::size_t total_symbols() const noexcept {
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
    void for_each_manager(F&& f) const noexcept {
        (f.template operator()<SubscriptionTs>(manager_<SubscriptionTs>()), ...);
    }

private:
    // ------------------------------------------------------------------------
    // Storage: one Manager per subscription type
    // ------------------------------------------------------------------------
    std::tuple<Manager<SubscriptionTs>...> managers_;

    // Timestamp of last progress (Registering, ACK processing, rejection processing, clear_all)
    std::uint64_t last_progress_ns_{lcr::system::monotonic_clock::instance().now_ns()};

    // ------------------------------------------------------------------------
    // Access helpers
    // ------------------------------------------------------------------------
    template<class RequestT>
    auto& manager_() noexcept {
        static_assert((std::is_same_v<RequestT, SubscriptionTs> || ...), "Controller does not manage this subscription type");
        return std::get<Manager<RequestT>>(managers_);
    }

    template<class RequestT>
    const auto& manager_() const noexcept {
        static_assert((std::is_same_v<RequestT, SubscriptionTs> || ...), "Controller does not manage this subscription type");
        return std::get<Manager<RequestT>>(managers_);
    }


    // ------------------------------------------------------------------------
    // Progress tracking
    // ------------------------------------------------------------------------
    void mark_progress_() noexcept {
        last_progress_ns_ = lcr::system::monotonic_clock::instance().now_ns();
    }
};

} // namespace wirekrak::core::protocol::subscription

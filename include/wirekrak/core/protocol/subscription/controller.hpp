#pragma once

#include <tuple>
#include <utility>
#include <type_traits>

#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/protocol/subscription/manager.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"
#include "wirekrak/core/meta/type_list.hpp"
#include "lcr/system/monotonic_clock.hpp"


namespace wirekrak::core::protocol::subscription {

// ============================================================
// Primary template (never used directly)
// ============================================================

template<class ProgressPolicy, class SubscriptionSet>
class Controller;


// ============================================================
// Specialization for type_list
// ============================================================

template<class ProgressPolicy, class... SubscriptionTs>
class Controller<ProgressPolicy, meta::type_list<SubscriptionTs...>> {
public:
    Controller() = default;

    // --------------------------------------------------------
    // Subscription registration
    // --------------------------------------------------------
    template<class DomainT>
    auto register_subscription(RequestSymbols symbols, ctrl::req_id_t req_id) noexcept {
        mark_progress_();
        return manager_<DomainT>()
            .register_subscription(std::move(symbols), req_id);
    }

    template<class DomainT>
    auto register_unsubscription(RequestSymbols symbols, ctrl::req_id_t req_id) noexcept {
        mark_progress_();
        return manager_<DomainT>()
            .register_unsubscription(std::move(symbols), req_id);
    }

    // --------------------------------------------------------
    // ACK handling
    // --------------------------------------------------------
    template<class DomainT>
    void process_subscribe_ack(ctrl::req_id_t req_id, Symbol symbol, bool success) noexcept {
        if (manager_<DomainT>()
                .process_subscribe_ack(req_id, symbol, success)) {
            mark_progress_();
        }
    }

    template<class DomainT>
    void process_unsubscribe_ack(ctrl::req_id_t req_id, Symbol symbol, bool success) noexcept {
        if (manager_<DomainT>()
                .process_unsubscribe_ack(req_id, symbol, success)) {
            mark_progress_();
        }
    }

    // --------------------------------------------------------
    // Rejection handling
    // --------------------------------------------------------
    [[nodiscard]]
    bool try_process_rejection(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        bool done = false;

        std::apply([&](auto&... mgr) {
            (( !done && (done = mgr.try_process_rejection(req_id, symbol)) ), ...);
        }, managers_);

        if (done) mark_progress_();
        return done;
    }

    // --------------------------------------------------------
    void clear_all() noexcept {
        std::apply([](auto&... mgr) {
            (mgr.clear_all(), ...);
        }, managers_);
        mark_progress_();
    }

    // --------------------------------------------------------
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
        if (is_quiescent()) return true;

        if constexpr (!ProgressPolicy::enabled)
            return false;

        const auto now = lcr::system::monotonic_clock::instance().now_ns();
        return (now - last_progress_ns_) > ProgressPolicy::timeout_ns;
    }

    // --------------------------------------------------------
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

    // --------------------------------------------------------
    template<class DomainT>
    const auto& manager_for() const noexcept {
        return manager_<DomainT>();
    }

    // --------------------------------------------------------
    template<class F>
    void for_each(F&& f) const noexcept {
        (f.template operator()<SubscriptionTs>(), ...);
    }

    template<class F>
    void for_each_manager(F&& f) const noexcept {
        (f.template operator()<SubscriptionTs>(manager_<SubscriptionTs>()), ...);
    }

private:
    std::tuple<Manager<SubscriptionTs>...> managers_;

    std::uint64_t last_progress_ns_{
        lcr::system::monotonic_clock::instance().now_ns()
    };

    // --------------------------------------------------------
    template<class T>
    auto& manager_() noexcept {
        static_assert((std::is_same_v<T, SubscriptionTs> || ...), "Controller does not manage this subscription type");
        return std::get<Manager<T>>(managers_);
    }

    template<class T>
    const auto& manager_() const noexcept {
        static_assert((std::is_same_v<T, SubscriptionTs> || ...), "Controller does not manage this subscription type");
        return std::get<Manager<T>>(managers_);
    }

    // --------------------------------------------------------
    void mark_progress_() noexcept {
        last_progress_ns_ = lcr::system::monotonic_clock::instance().now_ns();
    }
};

} // namespace wirekrak::core::protocol::subscription

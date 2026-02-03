#pragma once

#include "wirekrak/core/protocol/kraken/replay/Table.hpp"


namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace replay {

// =============================================================
// Database
// --------
// Keeps copies of subscription requests + callbacks,
// so they can be replayed after reconnect.
// Key features:
// - Type-safe: one Table per channel type
// - Uses compile-time routing via if constexpr
// =============================================================
class Database {
public:
    Database() = default;
    ~Database() = default;

    // INSERT / REPLACE ENTRY
    template<class RequestT, class Callback>
    inline void add(RequestT req, Callback&& cb) noexcept {
        subscription_table_for_<RequestT>().add(std::move(req), std::forward<Callback>(cb));
    }

    // REMOVE SYMBOLS FROM ENTRY
    template<class RequestT>
    inline void remove(RequestT req) noexcept {
        auto& table = subscription_table_for_<RequestT>();
        for (const auto& symbol : req.symbols) {
            table.erase_symbol(symbol);
        }
    }

    template<class RequestT>
    [[nodiscard]]
    inline bool contains(Symbol symbol) const noexcept {
        return subscription_table_for_<RequestT>().contains(symbol);
    }

    inline bool try_process_rejection(std::uint64_t req_id, Symbol symbol) noexcept {
        bool done = trade_.try_process_rejection(req_id, symbol);
        if (!done) {
            done = book_.try_process_rejection(req_id, symbol);
        }
        if (done) {
            WK_DEBUG("[REPLAY:DB] Processed rejection for symbol {" << symbol << "} (req_id=" << req_id << ")");
        }
        return done;
    }

    template<class RequestT>
    [[nodiscard]]
    inline std::vector<Subscription<RequestT>>&& take_subscriptions() noexcept {
        return subscription_table_for_<RequestT>().take_subscriptions();
    }

    // CLEAR ALL DATA
    inline void clear_all() noexcept {
        trade_.clear();
        book_.clear();
    }

private:
    Table<schema::trade::Subscribe> trade_;
    // Table<ticker::Subscribe> ticker_;
    Table<schema::book::Subscribe> book_;

private:
    // Helpers to get the correct handler table for a response type
    template<class RequestT>
    auto& subscription_table_for_() {
        if constexpr (channel_of_v<RequestT> == Channel::Trade) {
            return trade_;
        }
        // else if constexpr (channel_of_v<RequestT> == Channel::Ticker) {
        //     return ticker_;
        // }
        else if constexpr (channel_of_v<RequestT> == Channel::Book) {
            return book_;
        }
        else {
            static_assert(wirekrak::core::always_false<RequestT>, "Unsupported RequestT in subscription_table_for_()");
        }
    }

    template<class RequestT>
    const auto& subscription_table_for_() const {
        if constexpr (channel_of_v<RequestT> == Channel::Trade) {
            return trade_;
        }
        // else if constexpr (channel_of_v<RequestT> == Channel::Ticker) {
        //     return ticker_;
        //}
        else if constexpr (channel_of_v<RequestT> == Channel::Book) {
             return book_;
        }
        else {
            static_assert(wirekrak::core::always_false<RequestT>, "Unsupported RequestT in subscription_table_for_()");
        }
    }

};

} // namespace replay
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core

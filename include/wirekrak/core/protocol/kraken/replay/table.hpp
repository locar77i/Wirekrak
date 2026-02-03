#pragma once

#include <unordered_map>
#include <functional>
#include <cstdint>
#include <utility>

#include "wirekrak/core/protocol/kraken/replay/subscription.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace replay {

/*
    Table<RequestT>
    -----------------------------
    Stores outbound subscription requests along with their callbacks,
    allowing automatic replay after reconnect.

    Key features:
    - Type-safe: one DB per channel type (trade, ticker, book, …)
    - Stores a full request object (including symbols/settings)
    - Stores exactly one callback per request group_id
    - Supports replay, removal, iteration, etc.
*/
template<class RequestT>
class Table {
public:
    using ResponseT = typename channel_traits<RequestT>::response_type;
    using Callback = std::function<void(const ResponseT&)>;

public:
    Table() = default;

    // ------------------------------------------------------------
    // Add a new replay subscription
    // table_.emplace_back(request, callback)
    // ------------------------------------------------------------
    inline void add(RequestT req, Callback cb) {
        std::uint64_t req_id = req.req_id.has() ? req.req_id.value() : INVALID_REQ_ID;
        subscriptions_.emplace_back(std::move(req), std::move(cb));
        WK_TRACE("[REPLAY:TABLE] Added subscription with req_id=" << req_id << " and " << subscriptions_.back().request().symbols.size() << " symbol(s) " << " (total subscriptions=" << subscriptions_.size() << ")");
    }

    // ------------------------------------------------------------
    // Query
    // ------------------------------------------------------------
    [[nodiscard]]
    inline bool contains(Symbol symbol) const noexcept {
        for (auto& subscription : subscriptions_) {
            const auto& symbols = subscription.request().symbols;
            // Scan all symbol strings in the subscription
            for (const auto& sym : symbols) {
                if (symbol == sym) {
                    return true;
                }
            }
        }
        return false;
    }

    inline bool try_process_rejection(std::uint64_t req_id, Symbol symbol) noexcept {
        bool done = false;
        for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ++it) {
            bool done = it->try_process_rejection(req_id, symbol);
            if (done) {
                WK_TRACE("[REPLAY:TABLE] Rejected symbol {" << symbol << "} from subscription (req_id=" << req_id << ")");
                if (it->empty()) {
                    WK_TRACE("[REPLAY:TABLE] Removed empty subscription with req_id=" << req_id << " (total subscriptions=" << subscriptions_.size() - 1 << ")");
                    it = subscriptions_.erase(it);
                    break;
                }
            }
        }
        return done;
    }

    // ------------------------------------------------------------
    // Erase the first occurrence of a symbol from any subscription
    // This matches Kraken unsubscribe semantics:
    //   "unsubscribe(symbol)" removes that symbol from whichever
    //   subscription contains it.
    // ------------------------------------------------------------
    inline void erase_symbol(Symbol symbol) noexcept {
        for (std::size_t i = 0; i < subscriptions_.size(); i++) {
            auto& subscription = subscriptions_[i];
            bool removed = subscription.erase_symbol(symbol);
            if (removed) {
                if (subscription.empty()) {
                    subscriptions_.erase(subscriptions_.begin() + i);
                    WK_TRACE("[REPLAY:TABLE] Removed empty subscription with req_id=" << subscription.req_id() << "  (total subscriptions=" << subscriptions_.size() << ")");
                }
                return;
            }
        }
        WK_WARN("[REPLAY:TABLE] Failed to erase symbol {" << symbol << "} from any subscription (not found)");
    }

    // ------------------------------------------------------------
    // Debug/utility
    // ------------------------------------------------------------

    [[nodiscard]]
    inline bool empty() const noexcept {
        return subscriptions_.empty();
    }

    [[nodiscard]]
    inline size_t size() const noexcept {
        return subscriptions_.size();
    }

    inline void clear() noexcept {
        subscriptions_.clear();
    }

    [[nodiscard]]
    inline const std::vector<Subscription<RequestT>>& subscriptions() const noexcept {
        return subscriptions_;
    }

    [[nodiscard]]
    inline std::vector<Subscription<RequestT>>&& take_subscriptions() noexcept {
        return std::move(subscriptions_);
    }

private:
    std::vector<Subscription<RequestT>> subscriptions_;
};

} // namespace replay
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core

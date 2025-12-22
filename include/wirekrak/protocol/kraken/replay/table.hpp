#pragma once

#include <unordered_map>
#include <functional>
#include <cstdint>
#include <utility>

#include "wirekrak/protocol/kraken/replay/subscription.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak {
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
        subscriptions_.emplace_back(std::move(req), std::move(cb));
        WK_TRACE("[REPLAY] Added subscription with " << subscriptions_.back().request().symbols.size() << " symbol(s)  (total subscriptions=" << subscriptions_.size() << ")");
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
                WK_TRACE("[REPLAY] Erased symbol {" << symbol << "}" << " from subscription #" << i);
                if (subscription.empty()) {
                    subscriptions_.erase(subscriptions_.begin() + i);
                    WK_TRACE("[REPLAY] Removed empty subscription #" << i << "  (total subscriptions=" << subscriptions_.size() << ")");
                }
                return;
            }
        }
        WK_WARN("[REPLAY] Failed to erase symbol {" << symbol << "} from any subscription (not found)");
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
} // namespace wirekrak

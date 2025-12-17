#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <functional>

#include "wirekrak/protocol/kraken/trade/Response.hpp"
#include "wirekrak/protocol/kraken/channel_traits.hpp"
#include "wirekrak/core/symbol/intern.hpp"

using namespace wirekrak::protocol::kraken;


namespace wirekrak {

class Dispatcher {
public:
    // Callback type is per-response type
    template<class ResponseT>
    using Callback = std::function<void(const ResponseT&)>;

    // Add a handler for a symbol and response type
    template<class ResponseT, class Callback>
    inline void add_handler(Symbol symbol, Callback&& cb) {
        SymbolId symbol_id = intern_symbol(symbol);
        auto& table = handler_table_for_<ResponseT>();
        table[symbol_id].push_back(std::forward<Callback>(cb));
    }

    // Dispatch a message to the correct symbol listeners
    template<class ResponseT>
    inline void dispatch(const ResponseT& msg) {
        SymbolId sid = intern_symbol(msg.symbol);
        auto& table = handler_table_for_<ResponseT>();
        auto it = table.find(sid);
        if (it == table.end()) {
            return;
        }
        for (auto& cb : it->second) {
            cb(msg);
        }
    }

    template<class UnsubscribeAckT>
    inline void remove_symbol_handlers(Symbol symbol) {
        SymbolId sid = intern_symbol(symbol);
        auto& table = handler_table_for_<UnsubscribeAckT>();
        table.erase(sid);
    }

    // Clear everything (used when reconnecting or shutting down)
    inline void clear() noexcept {
        trade_handlers_.clear();
    }

private:
    std::unordered_map<SymbolId, std::vector<Callback<protocol::kraken::trade::Response>>> trade_handlers_;

private:
    // Helpers to get the correct handler table for a response type
    template<class MessageT>
    auto& handler_table_for_() {
        if constexpr (channel_of_v<MessageT> == Channel::Trade) {
            return trade_handlers_;
        }
        // else if constexpr (...) return ticker_handlers_;
        // else if constexpr (...) return book_handlers_;
    }

    template<class MessageT>
    const auto& handler_table_for_() const {
        if constexpr (channel_of_v<MessageT> == Channel::Trade) {
            return trade_handlers_;
        }
        // else if constexpr (...) return ticker_handlers_;
        // else if constexpr (...) return book_handlers_;
    }
};

} // namespace wirekrak

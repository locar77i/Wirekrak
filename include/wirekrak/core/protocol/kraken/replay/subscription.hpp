#pragma once

#include <functional>
#include <cstdint>
#include <utility>

#include "wirekrak/core/protocol/kraken/channel_traits.hpp"


namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace replay {

// ------------------------------------------------------------
// Per-entry object: stores request + callback + symbol ops
// ------------------------------------------------------------
template<class RequestT>
class Subscription {
public:
    using ResponseT = typename channel_traits<RequestT>::response_type;
    using Callback = std::function<void(const ResponseT&)>;

public:
    Subscription(RequestT req, Callback cb)
        : request_(std::move(req)), callback_(std::move(cb)) {}

    bool erase_symbol(Symbol symbol) {
        auto& symbols = request_.symbols;
        auto it = std::remove_if(symbols.begin(), symbols.end(),[&](const Symbol& sym) { return sym == symbol; });
        bool erased = (it != symbols.end());
        if (erased) symbols.erase(it, symbols.end());
        return erased;
    }

    bool empty() const noexcept {
        return request_.symbols.empty();
    }

    const RequestT& request() const noexcept { return request_; }
    RequestT& request() noexcept { return request_; }

    const Callback& callback() const noexcept { return callback_; }
    Callback& callback() noexcept { return callback_; }

private:
    RequestT request_;
    Callback callback_;
};

} // namespace replay
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core

#pragma once

#include <functional>
#include <cstdint>
#include <utility>

#include "wirekrak/core/protocol/kraken/channel_traits.hpp"


namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace replay {


constexpr std::uint64_t INVALID_REQ_ID = 0;

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

    [[nodiscard]]
    inline bool erase_symbol(Symbol symbol) {
        auto& symbols = request_.symbols;
        auto it = std::remove_if(symbols.begin(), symbols.end(),[&](const Symbol& sym) { return sym == symbol; });
        bool erased = (it != symbols.end());
        if (erased) {
            symbols.erase(it, symbols.end());
            WK_TRACE("[REPLAY:SUBSCRIPTION] Erased symbol {" << symbol << "}" << " from subscription (req_id=" << req_id() << ")");
        }
        return erased;
    }

    [[nodiscard]]
    inline bool try_process_rejection(ctrl::req_id_t req_id, Symbol symbol) noexcept {
        bool done = false;
        if (request_.req_id.has() && request_.req_id.value() == req_id) {
            // Match found → erase symbol
            done = erase_symbol(symbol);
        }
        return done;
    }

    [[nodiscard]]
    inline bool empty() const noexcept {
        return request_.symbols.empty();
    }

    [[nodiscard]] inline const RequestT& request() const noexcept { return request_; }
    [[nodiscard]] inline RequestT& request() noexcept { return request_; }

    [[nodiscard]] inline const Callback& callback() const noexcept { return callback_; }
    [[nodiscard]] inline Callback& callback() noexcept { return callback_; }

    [[nodiscard]] inline ctrl::req_id_t req_id() const noexcept {
        return request_.req_id.has() ? request_.req_id.value() : 0;
    }

private:
    RequestT request_;
    Callback callback_;
};

} // namespace replay
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core

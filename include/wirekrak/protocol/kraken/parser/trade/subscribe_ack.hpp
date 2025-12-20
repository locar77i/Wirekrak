#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/trade/subscribe_ack.hpp"
#include "wirekrak/protocol/kraken/parser/trade/detail/parse_ack_common.hpp"

namespace wirekrak::protocol::kraken::parser::trade {

struct subscribe_ack {
    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, kraken::trade::SubscribeAck& out) noexcept {
        out = kraken::trade::SubscribeAck{};
        return detail::parse_ack_common(root, "subscribe", out);
    }
};

} // namespace wirekrak::protocol::kraken::parser::trade

#pragma once

#include <string_view>

#include "wirekrak/core/protocol/kraken/schema/trade/subscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/parser/trade/detail/parse_ack_common.hpp"


namespace wirekrak::core::protocol::kraken::parser::trade {

struct subscribe_ack {
    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, schema::trade::SubscribeAck& out) noexcept {
        out = schema::trade::SubscribeAck{};
        return detail::parse_ack_common(root, "subscribe", out);
    }
};

} // namespace wirekrak::core::protocol::kraken::parser::trade

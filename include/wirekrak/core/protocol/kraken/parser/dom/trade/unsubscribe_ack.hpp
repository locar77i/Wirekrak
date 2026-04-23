#pragma once

#include <string_view>

#include "wirekrak/core/protocol/message_result.hpp"
#include "wirekrak/core/protocol/kraken/schema/trade/unsubscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/trade/detail/parse_ack_common.hpp"


namespace wirekrak::core::protocol::kraken::parser::dom::trade {

struct unsubscribe_ack {
    [[nodiscard]]
    static inline MessageResult parse(const simdjson::dom::element& root, schema::trade::UnsubscribeAck& out) noexcept {
        out = schema::trade::UnsubscribeAck{};
        return detail::parse_ack_common(root, "unsubscribe", out);
    }
};

} // namespace wirekrak::core::protocol::kraken::parser::dom::trade

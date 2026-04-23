#pragma once

#include <string_view>

#include "wirekrak/core/protocol/message_result.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/subscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/enums/channel.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/book/detail/parse_ack_common.hpp"


namespace wirekrak::core::protocol::kraken::parser::dom::book {

struct subscribe_ack {
    [[nodiscard]]
    static inline MessageResult parse(const simdjson::dom::element& root, schema::book::SubscribeAck& out) noexcept {
        out = schema::book::SubscribeAck{};
        return detail::parse_ack_common(root, "subscribe", out);
    }
};

} // namespace wirekrak::core::protocol::kraken::parser::dom::book

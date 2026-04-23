#pragma once

#include <string_view>

#include "wirekrak/core/protocol/message_result.hpp"
#include "wirekrak/core/protocol/kraken/schema/book/unsubscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/enums/channel.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/book/detail/parse_ack_common.hpp"


namespace wirekrak::core::protocol::kraken::parser::dom::book {

struct unsubscribe_ack {
    [[nodiscard]]
    static inline MessageResult parse(const simdjson::dom::element& root, schema::book::UnsubscribeAck& out) noexcept {
        out = schema::book::UnsubscribeAck{};
        return detail::parse_ack_common(root, "unsubscribe", out);
    }
};

} // namespace wirekrak::core::protocol::kraken::parser::dom::book

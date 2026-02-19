#pragma once

#include <string_view>

#include "wirekrak/core/protocol/kraken/schema/trade/unsubscribe_ack.hpp"
#include "wirekrak/core/protocol/kraken/parser/trade/detail/parse_ack_common.hpp"


namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace parser {
namespace trade {

struct unsubscribe_ack {
    [[nodiscard]]
    static inline Result parse(const simdjson::dom::element& root, schema::trade::UnsubscribeAck& out) noexcept {
        out = schema::trade::UnsubscribeAck{};
        return detail::parse_ack_common(root, "unsubscribe", out);
    }
};

} // namespace trade
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core

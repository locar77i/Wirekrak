#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/schema/trade/unsubscribe_ack.hpp"
#include "wirekrak/protocol/kraken/parser/trade/detail/parse_ack_common.hpp"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace parser {
namespace trade {

struct unsubscribe_ack {
    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, kraken::trade::UnsubscribeAck& out) noexcept {
        out = kraken::trade::UnsubscribeAck{};
        return detail::parse_ack_common(root, "unsubscribe", out);
    }
};

} // namespace trade
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

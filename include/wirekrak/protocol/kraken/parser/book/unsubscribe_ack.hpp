#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/book/unsubscribe_ack.hpp"
#include "wirekrak/protocol/kraken/enums/channel.hpp"
#include "wirekrak/protocol/kraken/parser/detail/book_ack_common.hpp"

#include "simdjson.h"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace parser {
namespace book {

struct unsubscribe_ack {
    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, kraken::book::UnsubscribeAck& out) noexcept {
            return parser::detail::parse_book_ack_common(root, "unsubscribe", out);
    }
};

} // namespace book
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

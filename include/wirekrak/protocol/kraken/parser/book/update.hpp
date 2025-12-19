#pragma once

#include "wirekrak/protocol/kraken/book/update.hpp"
#include "wirekrak/protocol/kraken/parser/book/detail/parse_payload_common.hpp"

#include "simdjson.h"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace parser {
namespace book {

struct update {
    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, kraken::book::Update& out) noexcept {
        return detail::parse_payload_common(root, "update", out);
    }
};

} // namespace book
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak


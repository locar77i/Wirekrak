#pragma once

#include "wirekrak/protocol/kraken/book/snapshot.hpp"
#include "wirekrak/protocol/kraken/parser/book/detail/parse_payload_common.hpp"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace parser {
namespace book {

struct snapshot {
    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, kraken::book::Snapshot& out) noexcept {
        return detail::parse_payload_common(root, "snapshot", out);
    }
};

} // namespace book
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak


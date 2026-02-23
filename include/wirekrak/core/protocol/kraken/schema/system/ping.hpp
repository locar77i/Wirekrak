#pragma once

#include <cstdint>
#include <string>
#include <cstring>

#include "wirekrak/core/protocol/kraken/request/validate.hpp"
#include "wirekrak/core/protocol/control/req_id.hpp"
#include "lcr/json.hpp"
#include "lcr/optional.hpp"

namespace wirekrak::core {
namespace protocol {
namespace kraken {
namespace schema {
namespace system {


// PRECONDITION:
//   Caller must provide a buffer of at least max_json_size() bytes.
//   No bounds checking is performed for performance reasons.
struct Ping {
    using control_tag = void;

    lcr::optional<ctrl::req_id_t> req_id{};

public:
    [[nodiscard]]
    static constexpr std::size_t max_json_size() noexcept {
        // Worst case:
        // {"method":"ping","req_id":18446744073709551615}
        return 64;
    }

    // Writes JSON into raw buffer.
    // Returns number of bytes written.
    // PRECONDITION: buffer_size >= max_json_size()
    [[nodiscard]]
    inline std::size_t write_json(char* buffer) const noexcept {
#ifndef NDEBUG
        request::validate_req_id(req_id);
#endif

        std::size_t pos = 0;

        // {"method":"ping"
        static constexpr char prefix[] = "{\"method\":\"ping\"";
        std::memcpy(buffer + pos, prefix, sizeof(prefix) - 1);
        pos += sizeof(prefix) - 1;

        // ,"req_id":<number>
        if (req_id.has()) {
            static constexpr char req_prefix[] = ",\"req_id\":";
            std::memcpy(buffer + pos, req_prefix, sizeof(req_prefix) - 1);
            pos += sizeof(req_prefix) - 1;

            pos += lcr::json::append(buffer + pos, req_id.value());
        }

        // }
        buffer[pos++] = '}';

#ifndef NDEBUG
        assert(pos <= max_json_size());
#endif

        return pos;
    }

#ifndef WIREKRAK_NO_ALLOCATIONS
    // Convenience method (allocating) for tests / logging.
    std::string to_json() const {
        char buffer[64]; // more than enough for this simple message. Worst case: {"method":"ping","req_id":18446744073709551615}
        std::size_t size = write_json(buffer);
        return std::string(buffer, size);
    }
#endif // WIREKRAK_NO_ALLOCATIONS
};

} // namespace system
} // namespace schema
} // namespace kraken
} // namespace protocol
} // namespace wirekrak::core

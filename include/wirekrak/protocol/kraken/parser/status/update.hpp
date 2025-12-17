#pragma once

#include <simdjson.h>

#include "wirekrak/protocol/kraken/status/update.hpp"
#include "lcr/log/logger.hpp"


namespace wirekrak {
namespace protocol {
namespace kraken {
namespace parser {
namespace status {

class update {
public:
    // Parse a Kraken "status" channel update
    //
    // Expected shape:
    // {
    //   "channel": "status",
    //   "type": "update",
    //   "data": [ { ... } ]
    // }
    [[nodiscard]]
    static inline bool parse(const simdjson::dom::element& root, kraken::status::Update& out) noexcept
    {
        using namespace simdjson;

        // data must be an array
        dom::array data;
        if (root["data"].get(data)) {
            WK_WARN("[STATUS] Missing 'data' array");
            return false;
        }

        // Kraken guarantees exactly one object
        auto it = data.begin();
        if (it == data.end()) {
            WK_WARN("[STATUS] Empty 'data' array");
            return false;
        }

        const dom::element& obj = *it;

        // system (required)
        std::string_view system_sv;
        if (obj["system"].get(system_sv)) {
            WK_WARN("[STATUS] Missing 'system'");
            return false;
        }
        out.system = to_system_state_enum_fast(system_sv);

        // api_version (required)
        std::string_view api_sv;
        if (obj["api_version"].get(api_sv)) {
            WK_WARN("[STATUS] Missing 'api_version'");
            return false;
        }
        out.api_version.assign(api_sv);

        // connection_id (required)
        if (obj["connection_id"].get(out.connection_id)) {
            WK_WARN("[STATUS] Missing 'connection_id'");
            return false;
        }

        // version (required)
        std::string_view version_sv;
        if (obj["version"].get(version_sv)) {
            WK_WARN("[STATUS] Missing 'version'");
            return false;
        }
        out.version.assign(version_sv);

        return true;
    }
};

} // namespace status
} // namespace parser
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

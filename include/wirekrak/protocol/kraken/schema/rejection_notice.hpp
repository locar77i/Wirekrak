#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <ostream>
#include <sstream>

#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/timestamp.hpp"
#include "lcr/optional.hpp"

namespace wirekrak {
namespace protocol {
namespace kraken {
namespace rejection {

// ===============================================
// REJECTION NOTICE
// Represents a failed request acknowledgement
//
// Failed Kraken acknowledgements are normalized into a single
// rejection::Notice type for consistent error handling.
// ===============================================
struct Notice {
    std::string error;
    lcr::optional<std::uint64_t> req_id;
    lcr::optional<Symbol> symbol;
    lcr::optional<Timestamp> time_in;
    lcr::optional<Timestamp> time_out;

    // ------------------------------------------------------------
    // Debug / inspection helper
    // ------------------------------------------------------------
    inline void dump(std::ostream& os) const {
        os << "[REJECTION] { " << "error=\"" << error << "\"";
        if (req_id.has()) {
            os << ", req_id=" << req_id.value();
        }
        if (symbol.has()) {
            os << ", symbol=" << symbol.value();
        }
        if (time_in.has()) {
            os << ", time_in=" << to_string(time_in.value());
        }
        if (time_out.has()) {
            os << ", time_out=" << to_string(time_out.value());
        }
        os << " }";
    }

    // ------------------------------------------------------------
    // String helper
    // ------------------------------------------------------------
    [[nodiscard]]
    inline std::string str() const {
        std::ostringstream oss;
        dump(oss);
        return oss.str();
    }
};

// Stream operator
inline std::ostream& operator<<(std::ostream& os, const Notice& n) {
    n.dump(os);
    return os;
}

} // namespace rejection
} // namespace kraken
} // namespace protocol
} // namespace wirekrak
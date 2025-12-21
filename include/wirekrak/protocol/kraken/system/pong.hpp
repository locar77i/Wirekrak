#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <ostream>
#include <sstream>

#include "wirekrak/core/timestamp.hpp"
#include "lcr/optional.hpp"

namespace wirekrak::protocol::kraken::system {

// ===============================================
// PING RESPONSE (pong)
// ===============================================
struct Pong {
    lcr::optional<bool> success;

    lcr::optional<std::uint64_t> req_id;

    // --- success-only fields ---
    std::vector<std::string> warnings;
    lcr::optional<Timestamp> time_in;
    lcr::optional<Timestamp> time_out;

    // --- error-only field ---
    lcr::optional<std::string> error;

    // ------------------------------------------------------------
    // Debug / diagnostic dump
    // ------------------------------------------------------------
    inline void dump(std::ostream& os) const {
        os << "[PONG] {\n";
        if (success.has()) {
            os << "  success: " << (success.value() ? "true" : "false") << "\n";
        }
        if (req_id.has()) {
            os << "  req_id: " << req_id.value() << "\n";
        }
        if (!warnings.empty()) {
            os << "  warnings:\n";
            for (const auto& w : warnings) {
                os << "    - " << w << "\n";
            }
        }
        if (time_in.has()) {
            os << "  time_in: " << to_string(time_in.value()) << "\n";
        }
        if (time_out.has()) {
            os << "  time_out: " << to_string(time_out.value()) << "\n";
        }
        if (error.has()) {
            os << "  error: " << error.value() << "\n";
        }
        os << "}\n";
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
inline std::ostream& operator<<(std::ostream& os, const Pong& p) {
    p.dump(os);
    return os;
}

} // namespace wirekrak::protocol::kraken::system

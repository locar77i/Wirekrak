#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <ostream>
#include <sstream>

#include "wirekrak/core/timestamp.hpp"
#include "lcr/optional.hpp"

namespace wirekrak {
namespace protocol {
namespace kraken {
namespace schema {
namespace system {

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
            os << "  time_in: " << wirekrak::to_string(time_in.value()) << "\n";
        }
        if (time_out.has()) {
            os << "  time_out: " << wirekrak::to_string(time_out.value()) << "\n";
        }
        if (error.has()) {
            os << "  error: " << error.value() << "\n";
        }
        os << "}\n";
    }

#ifndef NDEBUG
    // ---------------------------------------------------------
    // String helper (debug / logging)
    // NOTE: Allocates. Intended for debugging/logging only.
    // ---------------------------------------------------------
    [[nodiscard]]
    inline std::string str() const {
        std::ostringstream oss;
        dump(oss);
        return oss.str();
    }
#endif
};

// Stream operator<< delegates to dump(); allocation-free.
inline std::ostream& operator<<(std::ostream& os, const Pong& p) {
    p.dump(os);
    return os;
}

} // namespace system
} // namespace schema
} // namespace kraken
} // namespace protocol
} // namespace wirekrak

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <ostream>
#include <sstream>

#include "wirekrak/core/timestamp.hpp"
#include "lcr/optional.hpp"

namespace wirekrak::core {
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

    inline void to_json(std::ostream& os) const {
        bool first = true;

        os << "{";

        if (success.has()) {
            if (!first) os << ",";
            first = false;
            os << "\"success\":" << (success.value() ? "true" : "false");
        }

        if (req_id.has()) {
            if (!first) os << ",";
            first = false;
            os << "\"req_id\":" << req_id.value();
        }

        if (!warnings.empty()) {
            if (!first) os << ",";
            first = false;

            os << "\"warnings\":[";
            for (std::size_t i = 0; i < warnings.size(); ++i) {
                if (i) os << ",";
                os << "\"" << warnings[i] << "\"";
            }
            os << "]";
        }

        if (time_in.has()) {
            if (!first) os << ",";
            first = false;
            os << "\"time_in\":\""
            << wirekrak::core::to_string(time_in.value())
            << "\"";
        }

        if (time_out.has()) {
            if (!first) os << ",";
            first = false;
            os << "\"time_out\":\""
            << wirekrak::core::to_string(time_out.value())
            << "\"";
        }

        if (error.has()) {
            if (!first) os << ",";
            first = false;
            os << "\"error\":\"" << error.value() << "\"";
        }

        os << "}";
    }

    // ------------------------------------------------------------
    // Debug / diagnostic dump
    // ------------------------------------------------------------
    inline void dump(std::ostream& os) const {
        os << "[PONG] ";
        to_json(os);
    }

#ifndef NDEBUG
    // ---------------------------------------------------------
    // String helper (debug / logging)
    // NOTE: Allocates. Intended for debugging/logging only.
    // ---------------------------------------------------------
    [[nodiscard]]
    inline std::string str() const {
        std::ostringstream oss;
        to_json(oss);
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
} // namespace wirekrak::core

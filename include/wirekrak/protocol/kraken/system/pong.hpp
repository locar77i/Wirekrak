#pragma once

#include <cstdint>
#include <string>
#include <vector>

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
};

} // namespace wirekrak::protocol::kraken::system

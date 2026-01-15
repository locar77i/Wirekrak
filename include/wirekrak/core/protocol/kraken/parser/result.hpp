#pragma once

#include <cstdint>
#include <string_view>


namespace wirekrak::core::protocol::kraken::parser {

// ===============================================
// PARSER RESULT ENUM
// ===============================================
enum class Result : std::uint8_t {
    Ok,             // Parsed successfully
    InvalidSchema,  // Missing / malformed fields or structure
    InvalidValue    // Field present but semantically invalid
};

// -----------------------------------------------------------------------------
// Convert enum → string (for logging / diagnostics)
// -----------------------------------------------------------------------------
[[nodiscard]]
inline constexpr std::string_view to_string(Result r) noexcept {
    switch (r) {
        case Result::Ok:             return "ok";
        case Result::InvalidSchema:  return "invalid_schema";
        case Result::InvalidValue:   return "invalid_value";
        default:                     return "unknown";
    }
}

} // namespace wirekrak::core::protocol::kraken::parser

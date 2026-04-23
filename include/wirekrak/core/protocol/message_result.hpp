#pragma once

#include <cstdint>
#include <string_view>


namespace wirekrak::core::protocol {

// ===============================================
// PARSER RESULT ENUM
// ===============================================
enum class MessageResult : std::uint8_t {
    // ---- Parsing domain (0–7) ----
    Ignored        = 0,            // Not applicable / unknown method or channel
    InvalidJson    = 1,            // Structural failure
    InvalidSchema  = 2,            // Schema validation failure (missing required field, type mismatch, etc.)
    InvalidValue   = 3,            // Field present but semantically invalid
    Parsed         = 4,            // Parsed successfully

    // ---- Delivery domain (8–15) ----
    Delivered      = 8,            // Parsed successfully and delivered to the next stage (e.g. ring buffer)
    Backpressure   = 9             // Delivery failure due to backpressure (e.g. full ring buffer)
};

// -----------------------------------------------------------------------------
// Convert enum → string (for logging / diagnostics)
// -----------------------------------------------------------------------------
[[nodiscard]]
inline constexpr std::string_view to_string(MessageResult r) noexcept {
    switch (r) {
        case MessageResult::Ignored:        return "Ignored";
        case MessageResult::InvalidJson:    return "InvalidJson";
        case MessageResult::InvalidSchema:  return "InvalidSchema";
        case MessageResult::InvalidValue:   return "InvalidValue";
        case MessageResult::Parsed:         return "Parsed";
        case MessageResult::Delivered:      return "Delivered";
        case MessageResult::Backpressure:   return "Backpressure";
        default:                            return "unknown";
    }
}

} // namespace wirekrak::core::protocol

#pragma once

#include <string_view>

#include "wirekrak/core/protocol/message_result.hpp"
#include "wirekrak/core/protocol/kraken/enums/method.hpp"
#include "wirekrak/core/protocol/kraken/enums/channel.hpp"
#include "wirekrak/core/protocol/kraken/enums/side.hpp"
#include "wirekrak/core/protocol/kraken/enums/order_type.hpp"
#include "wirekrak/core/protocol/kraken/enums/payload_type.hpp"
#include "wirekrak/core/protocol/kraken/enums/system_state.hpp"
#include "wirekrak/core/protocol/kraken/parser/dom/helpers.hpp"
#include "wirekrak/core/symbol.hpp"
#include "wirekrak/core/timestamp.hpp"

#include "simdjson.h"

/*
================================================================================
Kraken Parsing Adapters (Domain-Level Converters)
================================================================================

This header defines domain-aware parsing adapters that convert validated JSON
primitives into strongly typed Kraken protocol objects.

Adapters sit between:
  • Low-level JSON helpers (helper::parse_*), and
  • High-level message parsers responsible for logging and routing

Responsibilities:
  • Convert primitive JSON fields into domain types (Symbol, Side, Timestamp…)
  • Enforce semantic constraints (non-empty strings, valid enum values)
  • Reject invalid or unknown domain values
  • Preserve strict schema compliance
  • Remain allocation-conscious and exception-free

Design principles:
  • Adapters do NOT perform logging
  • Adapters do NOT inspect message-level structure
  • Adapters enforce domain invariants only
  • Optional fields are handled explicitly and strictly
  • Unknown enum values are always rejected

Separation of concerns:
  - helper::*   → JSON mechanics and type extraction
  - adapter::*  → Domain semantics and validation
  - parser::*   → Message orchestration, logging, and control flow

This layered design ensures correctness, performance, and maintainability
across all Kraken WebSocket protocol parsers.

================================================================================
*/


namespace wirekrak::core::protocol::kraken::parser::dom::adapter {

// ------------------------------------------------------------
// Method
// ------------------------------------------------------------
[[nodiscard]]
inline MessageResult parse_method_required(const simdjson::dom::element& root, Method& out) noexcept {
    // Required string field
    std::string_view sv;
    auto r = helper::parse_string_required(root, "method", sv);
    if (r != MessageResult::Parsed) {
        return r;
    }
    // Convert to enum
    out = to_method_enum_fast(sv);
    // Present but unknown method
    if (out == Method::Unknown) {
        return MessageResult::InvalidValue;
    }
    return MessageResult::Parsed;
}


// ------------------------------------------------------------
// Channel
// ------------------------------------------------------------
[[nodiscard]]
inline MessageResult parse_channel_required(const simdjson::dom::element& root, Channel& out) noexcept {
    // Required string field
    std::string_view sv;
    auto r = helper::parse_string_required(root, "channel", sv);
    if (r != MessageResult::Parsed) {
        return r;
    }
    // Convert to enum
    out = to_channel_enum_fast(sv);
    if (out == Channel::Unknown) {
        return MessageResult::InvalidValue;
    }
    return MessageResult::Parsed;
}


// ------------------------------------------------------------
// Symbol
// ------------------------------------------------------------
[[nodiscard]]
inline MessageResult parse_symbol_required(const simdjson::dom::object& obj, const char* key, Symbol& out) noexcept {
    // Required string field
    std::string_view sv;
    auto r = helper::parse_string_required(obj, key, sv);
    if (r != MessageResult::Parsed) {
        return r;
    }
    // Enforce non-empty
    if (sv.empty()) {
        return MessageResult::InvalidValue;
    }
    // Valid symbol
    out = Symbol{std::string(sv)};
    return MessageResult::Parsed;
}

[[nodiscard]]
inline MessageResult parse_symbol_optional(const simdjson::dom::object& obj, const char* key, lcr::optional<Symbol>& out) noexcept {
    // Always reset output (streaming safety)
    out.reset();
    // Parse optional string field
    bool presence = false;
    std::string_view sv;
    auto r = helper::parse_string_optional(obj, key, sv, presence);
    if (r != MessageResult::Parsed) {
        return r; // InvalidSchema bubbles up
    }
    // Field not present → OK (optional)
    if (!presence) {
        return MessageResult::Parsed;
    }
    // Field present but empty → invalid value
    if (sv.empty()) {
        return MessageResult::InvalidValue;
    }
    // Valid symbol
    out = Symbol{std::string(sv)};
    return MessageResult::Parsed;
}

// ------------------------------------------------------------
// Side
// ------------------------------------------------------------
[[nodiscard]]
inline MessageResult parse_side_required(const simdjson::dom::object& obj, const char* key, Side& out) noexcept {
    // Required string field
    std::string_view sv;
    auto r = helper::parse_string_required(obj, key, sv);
    if (r != MessageResult::Parsed) {
        return r;
    }
    // Enforce non-empty
    if (sv.empty()) {
        return MessageResult::InvalidValue;
    }
    // Convert to enum
    out = to_side_enum_fast(sv);
    if (out == Side::Unknown) {
        return MessageResult::InvalidValue;
    }
    return MessageResult::Parsed;
}

// ------------------------------------------------------------
// Order type (optional)
// ------------------------------------------------------------
[[nodiscard]]
inline MessageResult parse_order_type_optional(const simdjson::dom::object& obj, const char* key, lcr::optional<OrderType>& out) noexcept {
    // Always reset output (streaming safety)
    out.reset();
    // Parse optional string field
    bool presence = false;
    std::string_view sv;
    auto r = helper::parse_string_optional(obj, key, sv, presence);
    if (r != MessageResult::Parsed) {
        return r; // InvalidSchema bubbles up
    }
    // Field not present → OK (optional)
    if (!presence) {
        return MessageResult::Parsed;
    }
    // Convert to enum
    OrderType t = to_order_type_enum_fast(sv);
    // Present but invalid value
    if (t == OrderType::Unknown) {
        return MessageResult::InvalidValue;
    }
    out = t;
    return MessageResult::Parsed;
}

// ------------------------------------------------------------
// PayloadType (snapshot / update)
// ------------------------------------------------------------
[[nodiscard]]
inline MessageResult parse_payload_type_required(const simdjson::dom::element& obj, const char* key, kraken::PayloadType& out) noexcept {
    // Required string field
    std::string_view sv;
    auto r = helper::parse_string_required(obj, key, sv);
    if (r != MessageResult::Parsed) {
        return r; // InvalidSchema bubbles up
    }
    // Present but empty → invalid value
    if (sv.empty()) {
        return MessageResult::InvalidValue;
    }
    // Convert using fast path
    out = kraken::to_payload_type_enum_fast(sv);
    // Unknown payload type → invalid value
    if (out == kraken::PayloadType::Unknown) {
        return MessageResult::InvalidValue;
    }
    return MessageResult::Parsed;
}

// ------------------------------------------------------------
// SystemState (status channel)
[[nodiscard]]
inline MessageResult parse_system_state_required(const simdjson::dom::object& obj, const char* key, SystemState& out) noexcept {
    // Required string field
    std::string_view sv;
    auto r = helper::parse_string_required(obj, key, sv);
    if (r != MessageResult::Parsed) {
        return r;
    }
    // Enforce non-empty
    if (sv.empty()) {
        return MessageResult::InvalidValue;
    }
    // Convert to enum
    out = to_system_state_enum_fast(sv);
    if (out == SystemState::Unknown) {
        return MessageResult::InvalidValue;
    }
    return MessageResult::Parsed;
}


// ------------------------------------------------------------
// Timestamp
// ------------------------------------------------------------
[[nodiscard]]
inline MessageResult parse_timestamp_required(const simdjson::dom::object& obj, const char* key, Timestamp& out) noexcept {
    std::string_view sv;
    // Required string field
    auto r = helper::parse_string_required(obj, key, sv);
    if (r != MessageResult::Parsed) {
        return r; // InvalidSchema bubbles up
    }
    // Enforce non-empty
    if (sv.empty()) {
        return MessageResult::InvalidValue;
    }
    // Parse RFC3339 timestamp
    if (!parse_rfc3339(sv, out)) {
        return MessageResult::InvalidValue;
    }
    return MessageResult::Parsed;
}

[[nodiscard]]
inline MessageResult parse_timestamp_optional(const simdjson::dom::object& obj, const char* key, lcr::optional<Timestamp>& out) noexcept {
    out.reset();
    bool presence = false;
    // Optional string field
    std::string_view sv;
    auto r = helper::parse_string_optional(obj, key, sv, presence);
    if (r != MessageResult::Parsed) {
        return r; // InvalidSchema
    }
    // Field not present → OK (optional)
    if (!presence) {
        return MessageResult::Parsed; // optional, not present
    }
    // Enforce non-empty
    Timestamp ts;
    if (!parse_rfc3339(sv, ts)) {
        return MessageResult::InvalidValue;
    }
    out = ts;
    return MessageResult::Parsed;
}


} // namespace wirekrak::core::protocol::kraken::parser::dom::adapter

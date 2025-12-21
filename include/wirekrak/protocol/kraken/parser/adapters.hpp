#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/enums/method.hpp"
#include "wirekrak/protocol/kraken/enums/channel.hpp"
#include "wirekrak/protocol/kraken/enums/side.hpp"
#include "wirekrak/protocol/kraken/enums/order_type.hpp"
#include "wirekrak/protocol/kraken/enums/payload_type.hpp"
#include "wirekrak/protocol/kraken/parser/result.hpp"
#include "wirekrak/protocol/kraken/parser/helpers.hpp"
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


namespace wirekrak::protocol::kraken::parser::adapter {

// ------------------------------------------------------------
// Method
// ------------------------------------------------------------
[[nodiscard]]
inline parser::Result parse_method_required(const simdjson::dom::element& root, Method& out) noexcept {
    // Required string field
    std::string_view sv;
    auto r = parser::helper::parse_string_required(root, "method", sv);
    if (r != parser::Result::Ok) {
        return r;
    }
    // Convert to enum
    out = to_method_enum_fast(sv);
    // Present but unknown method
    if (out == Method::Unknown) {
        return parser::Result::InvalidValue;
    }
    return parser::Result::Ok;
}


// ------------------------------------------------------------
// Channel
// ------------------------------------------------------------
[[nodiscard]]
inline parser::Result parse_channel_required(const simdjson::dom::element& root, Channel& out) noexcept {
    // Required string field
    std::string_view sv;
    auto r = parser::helper::parse_string_required(root, "channel", sv);
    if (r != parser::Result::Ok) {
        return r;
    }
    // Convert to enum
    out = to_channel_enum_fast(sv);
    if (out == Channel::Unknown) {
        return parser::Result::InvalidValue;
    }
    return parser::Result::Ok;
}


// ------------------------------------------------------------
// Symbol
// ------------------------------------------------------------
[[nodiscard]]
inline parser::Result parse_symbol_required(const simdjson::dom::object& obj, const char* key, Symbol& out) noexcept {
    // Required string field
    std::string_view sv;
    auto r = parser::helper::parse_string_required(obj, key, sv);
    if (r != parser::Result::Ok) {
        return r;
    }
    // Enforce non-empty
    if (sv.empty()) {
        return parser::Result::InvalidValue;
    }
    // Valid symbol
    out = Symbol{std::string(sv)};
    return parser::Result::Ok;
}

[[nodiscard]]
inline parser::Result parse_symbol_optional(const simdjson::dom::object& obj, const char* key, lcr::optional<Symbol>& out) noexcept {
    // Always reset output (streaming safety)
    out.reset();
    // Parse optional string field
    bool presence = false;
    std::string_view sv;
    auto r = parser::helper::parse_string_optional(obj, key, sv, presence);
    if (r != parser::Result::Ok) {
        return r; // InvalidSchema bubbles up
    }
    // Field not present → OK (optional)
    if (!presence) {
        return parser::Result::Ok;
    }
    // Field present but empty → invalid value
    if (sv.empty()) {
        return parser::Result::InvalidValue;
    }
    // Valid symbol
    out = Symbol{std::string(sv)};
    return parser::Result::Ok;
}

// ------------------------------------------------------------
// Side
// ------------------------------------------------------------
[[nodiscard]]
inline parser::Result parse_side_required(const simdjson::dom::object& obj, const char* key, Side& out) noexcept {
    // Required string field
    std::string_view sv;
    auto r = parser::helper::parse_string_required(obj, key, sv);
    if (r != parser::Result::Ok) {
        return r;
    }
    // Enforce non-empty
    if (sv.empty()) {
        return parser::Result::InvalidValue;
    }
    // Convert to enum
    out = to_side_enum_fast(sv);
    if (out == Side::Unknown) {
        return parser::Result::InvalidValue;
    }
    return parser::Result::Ok;
}

// ------------------------------------------------------------
// Order type (optional)
// ------------------------------------------------------------
[[nodiscard]]
inline parser::Result parse_order_type_optional(const simdjson::dom::object& obj, const char* key, lcr::optional<OrderType>& out) noexcept {
    // Always reset output (streaming safety)
    out.reset();
    // Parse optional string field
    bool presence = false;
    std::string_view sv;
    auto r = parser::helper::parse_string_optional(obj, key, sv, presence);
    if (r != parser::Result::Ok) {
        return r; // InvalidSchema bubbles up
    }
    // Field not present → OK (optional)
    if (!presence) {
        return parser::Result::Ok;
    }
    // Convert to enum
    OrderType t = to_order_type_enum_fast(sv);
    // Present but invalid value
    if (t == OrderType::Unknown) {
        return parser::Result::InvalidValue;
    }
    out = t;
    return parser::Result::Ok;
}

// ------------------------------------------------------------
// PayloadType (snapshot / update)
// ------------------------------------------------------------
[[nodiscard]]
inline parser::Result parse_payload_type_required(const simdjson::dom::element& obj,const char* key,kraken::PayloadType& out) noexcept {
    // Required string field
    std::string_view sv;
    auto r = helper::parse_string_required(obj, key, sv);
    if (r != parser::Result::Ok) {
        return r; // InvalidSchema bubbles up
    }
    // Present but empty → invalid value
    if (sv.empty()) {
        return parser::Result::InvalidValue;
    }
    // Convert using fast path
    out = kraken::to_payload_type_enum_fast(sv);
    // Unknown payload type → invalid value
    if (out == kraken::PayloadType::Unknown) {
        return parser::Result::InvalidValue;
    }
    return parser::Result::Ok;
}


// ------------------------------------------------------------
// Timestamp
// ------------------------------------------------------------
[[nodiscard]]
inline parser::Result parse_timestamp_required(const simdjson::dom::object& obj, const char* key, Timestamp& out) noexcept {
    std::string_view sv;
    // Required string field
    auto r = parser::helper::parse_string_required(obj, key, sv);
    if (r != parser::Result::Ok) {
        return r; // InvalidSchema bubbles up
    }
    // Enforce non-empty
    if (sv.empty()) {
        return parser::Result::InvalidValue;
    }
    // Parse RFC3339 timestamp
    if (!parse_rfc3339(sv, out)) {
        return parser::Result::InvalidValue;
    }
    return parser::Result::Ok;
}

[[nodiscard]]
inline parser::Result parse_timestamp_optional(const simdjson::dom::object& obj, const char* key, lcr::optional<Timestamp>& out) noexcept {
    out.reset();
    bool presence = false;
    // Optional string field
    std::string_view sv;
    auto r = parser::helper::parse_string_optional(obj, key, sv, presence);
    if (r != parser::Result::Ok) {
        return r; // InvalidSchema
    }
    // Field not present → OK (optional)
    if (!presence) {
        return parser::Result::Ok; // optional, not present
    }
    // Enforce non-empty
    Timestamp ts;
    if (!parse_rfc3339(sv, ts)) {
        return parser::Result::InvalidValue;
    }
    out = ts;
    return parser::Result::Ok;
}


} // namespace wirekrak::protocol::kraken::parser::adapter

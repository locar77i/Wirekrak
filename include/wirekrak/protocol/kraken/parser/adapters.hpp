#pragma once

#include <string_view>

#include "wirekrak/protocol/kraken/enums/method.hpp"
#include "wirekrak/protocol/kraken/enums/channel.hpp"
#include "wirekrak/protocol/kraken/enums/side.hpp"
#include "wirekrak/protocol/kraken/enums/order_type.hpp"
#include "wirekrak/protocol/kraken/enums/payload_type.hpp"
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
inline bool parse_method_required(const simdjson::dom::element& root, Method& out) noexcept {
    std::string_view sv;
    if (!parser::helper::parse_string_required(root, "method", sv)) {
        return false;
    }
    out = to_method_enum_fast(sv);
    return out != Method::Unknown;
}


// ------------------------------------------------------------
// Channel
// ------------------------------------------------------------
[[nodiscard]]
inline bool parse_channel_required(const simdjson::dom::element& root, Channel& out) noexcept {
    std::string_view sv;
    if (!parser::helper::parse_string_required(root, "channel", sv)) {
        return false;
    }
    out = to_channel_enum_fast(sv);
    return out != Channel::Unknown;
}


// ------------------------------------------------------------
// Symbol
// ------------------------------------------------------------
[[nodiscard]]
inline bool parse_symbol_required(const simdjson::dom::object& obj, const char* key, Symbol& out) noexcept {
    std::string_view sv;
    if (!parser::helper::parse_string_required(obj, key, sv)) {
        return false;
    }
    if (sv.empty()) {
        return false;
    }
    out = Symbol{std::string(sv)};
    return true;
}

[[nodiscard]]
inline bool parse_symbol_optional(const simdjson::dom::object& obj, const char* key, lcr::optional<Symbol>& out) noexcept {
    out.reset();
    std::string_view sv;
    if (!parser::helper::parse_string_optional(obj, key, sv)) {
        return false;
    }
    if (sv.empty()) {
        return true; // optional, not present
    }
    out = Symbol{std::string(sv)};
    return true;
}

// ------------------------------------------------------------
// Side
// ------------------------------------------------------------
[[nodiscard]]
inline bool parse_side_required(const simdjson::dom::object& obj, const char* key, Side& out) noexcept {
    std::string_view sv;
    if (!parser::helper::parse_string_required(obj, key, sv)) {
        return false;
    }
    if (sv.empty()) {
        return false;
    }
    out = to_side_enum_fast(sv);
    return out != Side::Unknown;
}

// ------------------------------------------------------------
// Order type (optional)
// ------------------------------------------------------------
[[nodiscard]]
inline bool parse_order_type_optional(const simdjson::dom::object& obj, const char* key, lcr::optional<OrderType>& out) noexcept {
    out.reset();
    std::string_view sv;
    if (!parser::helper::parse_string_optional(obj, key, sv)) {
        return false;
    }
    if (sv.empty()) {
        return true; // optional, not present
    }
    OrderType t = to_order_type_enum_fast(sv);
    if (t == OrderType::Unknown) {
        return false;
    }
    out = t;
    return true;
}

// ------------------------------------------------------------
// PayloadType (snapshot / update)
// ------------------------------------------------------------
[[nodiscard]]
inline bool parse_payload_type_required(const simdjson::dom::element& obj,const char* key,kraken::PayloadType& out) noexcept {
    std::string_view sv;
    // Extract string
    if (!helper::parse_string_required(obj, key, sv)) {
        return false; // invalid
    }
    // Enforce non-empty
    if (sv.empty()) {
        return false; // invalid
    }
    // Convert using fast path
    out = kraken::to_payload_type_enum_fast(sv);
    return out != kraken::PayloadType::Unknown;
}


// ------------------------------------------------------------
// Timestamp
// ------------------------------------------------------------
[[nodiscard]]
inline bool parse_timestamp_required(const simdjson::dom::object& obj, const char* key, Timestamp& out) noexcept {
    std::string_view sv;
    if (!parser::helper::parse_string_required(obj, key, sv)) {
        return false;
    }
    if (sv.empty()) {
        return false;
    }
    return parse_rfc3339(sv, out);
}

[[nodiscard]]
inline bool parse_timestamp_optional(const simdjson::dom::object& obj, const char* key, lcr::optional<Timestamp>& out) noexcept {
    out.reset();
    std::string_view sv;
    if (!parser::helper::parse_string_optional(obj, key, sv)) {
        return false;
    }
    if (sv.empty()) {
        return true;
    }
    Timestamp ts;
    if (!parse_rfc3339(sv, ts)) {
        return false;
    }
    out = ts;
    return true;
}


} // namespace wirekrak::protocol::kraken::parser::adapter

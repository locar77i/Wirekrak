#pragma once

#include <string_view>
#include <vector>

#include "wirekrak/core/protocol/message_result.hpp"
#include "wirekrak/core/timestamp.hpp"
#include "lcr/optional.hpp"

#include "simdjson.h"

/*
================================================================================
Kraken JSON Parsing Helpers (Low-Level Primitives)
================================================================================

This header defines low-level, allocation-free helper functions used by Kraken
protocol parsers to safely extract primitive JSON values from simdjson DOM
elements.

Responsibilities:
  • Enforce basic JSON structural rules (object presence, type correctness)
  • Parse primitive field types (bool, integer, string)
  • Provide strict optional-field handling semantics
  • Never allocate memory
  • Never perform domain validation
  • Never log or report errors

Design principles:
  • Helpers are schema-agnostic and reusable across all Kraken channels
  • Empty strings and semantic validation are intentionally NOT handled here
  • All functions return boolean success/failure and are [[nodiscard]]
  • All helpers are noexcept and side-effect free on failure

These primitives form the foundation upon which higher-level adapters
(domain-aware parsing) and parsers (logging + control flow) are built.

IMPORTANT:
  - Helpers MUST NOT interpret values semantically
  - Helpers MUST NOT emit logs
  - Helpers MUST NOT throw exceptions

================================================================================
*/


namespace wirekrak::core::protocol::kraken::parser::dom::helper {

// ============================================================================
// ROOT TYPE
// ============================================================================

[[nodiscard]]
inline MessageResult require_object(const simdjson::dom::element& root) noexcept {
    return (root.type() == simdjson::dom::element_type::OBJECT) ? MessageResult::Parsed : MessageResult::InvalidSchema;
}

// ------------------------------------------------------------
// REQUIRED OBJECT FIELD
// ------------------------------------------------------------
[[nodiscard]]
inline MessageResult parse_object_required(const simdjson::dom::element& parent, const char* key, simdjson::dom::element& out) noexcept {
    // ensure parent is object
    auto r = require_object(parent);
    if (r != MessageResult::Parsed) {
        return r;
    }
    // lookup field
    auto field = parent[key];
    if (field.error()) {
        return MessageResult::InvalidSchema;
    }
    // extract value
    out = field.value();
    // Field must be an object
    r = require_object(out);
    if (r != MessageResult::Parsed) {
        return r;
    }
    return MessageResult::Parsed;
}

// ------------------------------------------------------------
// OPTIONAL OBJECT FIELD
// ------------------------------------------------------------
[[nodiscard]]
inline MessageResult parse_object_optional(const simdjson::dom::element& parent, const char* key, simdjson::dom::element& out, bool& present) noexcept {
    // Default: not present
    present = false;
    // Parent must be an object
    if (require_object(parent) != MessageResult::Parsed) {
        return MessageResult::InvalidSchema;
    }
    // Lookup field
    auto field = parent[key];
    if (field.error()) {
        return MessageResult::Parsed; // optional, not present
    }
    // Extract value
    out = field.value();
    // Field must be an object
    if (require_object(out) != MessageResult::Parsed) {
        return MessageResult::InvalidSchema;
    }
    present = true;
    return MessageResult::Parsed;
}

// ------------------------------------------------------------
// REQUIRED ARRAY FIELD
// ------------------------------------------------------------
[[nodiscard]]
inline MessageResult parse_array_required(const simdjson::dom::element& parent, const char* key, simdjson::dom::array& out) noexcept {
    // Parent must be an object
    if (require_object(parent) != MessageResult::Parsed) {
        return MessageResult::InvalidSchema;
    }
    // Lookup field
    auto field = parent[key];
    if (field.error()) {
        return MessageResult::InvalidSchema;
    }
    // Extract array
    if (field.get(out)) {
        return MessageResult::InvalidSchema;
    }
    return MessageResult::Parsed;
}

// ------------------------------------------------------------
// OPTIONAL ARRAY FIELD
// ------------------------------------------------------------
[[nodiscard]]
inline MessageResult parse_array_optional(const simdjson::dom::element& parent, const char* key, simdjson::dom::array& out, bool& present) noexcept {
    present = false;
    // Parent must be object
    if (require_object(parent) != MessageResult::Parsed) {
        return MessageResult::InvalidSchema;
    }
    // Lookup field
    auto field = parent[key];
    if (field.error()) {
        return MessageResult::Parsed; // optional, not present
    }
    // Extract array
    if (field.get(out)) {
        return MessageResult::InvalidSchema;
    }
    present = true;
    return MessageResult::Parsed;
}


[[nodiscard]]
inline bool parse_string_equals_required(const simdjson::dom::element& obj, const char* key, std::string_view expected) noexcept {
    auto r = require_object(obj);
    if (r != MessageResult::Parsed) {
        return false;
    }
    // Get string field
    std::string_view sv;
    if (obj[key].get(sv)) {
        return false; // missing or wrong type
    }
    return sv == expected;
}


// ============================================================================
// REQUIRED FIELD PARSERS
// ============================================================================

[[nodiscard]]
inline MessageResult parse_bool_required(const simdjson::dom::element& obj, const char* key, bool& out) noexcept {
    // Parent must be an object
    if (require_object(obj) != MessageResult::Parsed) {
        return MessageResult::InvalidSchema;
    }
    // Lookup field
    auto field = obj[key];
    if (field.error()) {
        return MessageResult::InvalidSchema;
    }
    // Extract boolean
    if (field.get(out)) {
        return MessageResult::InvalidSchema;
    }
    return MessageResult::Parsed;
}

[[nodiscard]]
inline MessageResult parse_uint64_required(const simdjson::dom::element& obj, const char* key, std::uint64_t& out) noexcept {
    // Parent must be an object
    if (require_object(obj) != MessageResult::Parsed) {
        return MessageResult::InvalidSchema;
    }
    // Lookup field
    auto field = obj[key];
    if (field.error()) {
        return MessageResult::InvalidSchema;
    }
    // Extract value
    if (field.get(out)) {
        return MessageResult::InvalidSchema;
    }
    return MessageResult::Parsed;
}

[[nodiscard]]
inline MessageResult parse_double_required(const simdjson::dom::element& obj, const char* key, double& out) noexcept {
    // Parent must be an object
    if (require_object(obj) != MessageResult::Parsed) {
        return MessageResult::InvalidSchema;
    }
    // Lookup field
    auto field = obj[key];
    if (field.error()) {
        return MessageResult::InvalidSchema;
    }
    // Extract value
    if (field.get(out)) {
        return MessageResult::InvalidSchema;
    }
    return MessageResult::Parsed;
}

[[nodiscard]]
inline MessageResult parse_string_required(const simdjson::dom::element& obj, const char* key, std::string_view& out) noexcept {
    // Parent must be an object
    if (require_object(obj) != MessageResult::Parsed) {
        return MessageResult::InvalidSchema;
    }
    // Lookup field
    auto field = obj[key];
    if (field.error()) {
        return MessageResult::InvalidSchema;
    }
    // Extract string
    if (field.get(out)) {
        return MessageResult::InvalidSchema;
    }
    return MessageResult::Parsed;
}

// ============================================================================
// OPTIONAL FIELD PARSERS
// ============================================================================
[[nodiscard]]
inline MessageResult parse_string_optional(const simdjson::dom::element& obj, const char* key, std::string_view& out, bool& presence) noexcept {
    // Default outputs
    presence = false;
    out = std::string_view{};
    // Parent must be an object
    if (require_object(obj) != MessageResult::Parsed) {
        return MessageResult::InvalidSchema;
    }
    // Lookup field
    auto field = obj[key];
    if (field.error()) {
        // Optional field not present
        return MessageResult::Parsed;
    }
    // Field is present
    presence = true;
    // Extract string
    if (field.get(out)) {
        return MessageResult::InvalidSchema;
    }
    return MessageResult::Parsed;
}

[[nodiscard]]
inline MessageResult parse_string_optional(const simdjson::dom::object& obj, const char* key, lcr::optional<std::string>& out) noexcept {
    // Always reset output (streaming safety)
    out.reset();
    // Parse optional string field
    bool presence = false;
    std::string_view sv;
    auto r = parse_string_optional(obj, key, sv, presence);
    if (r != MessageResult::Parsed) {
        return r; // InvalidSchema bubbles up
    }
    // Field not present → OK (optional)
    if (!presence) {
        return MessageResult::Parsed;
    }
    // Convert to string
    out = std::string(sv);

    return MessageResult::Parsed;
}

[[nodiscard]]
inline MessageResult parse_bool_optional(const simdjson::dom::element& obj, const char* key, lcr::optional<bool>& out) noexcept {
    // Always reset output (streaming safety)
    out.reset();
    // Parent must be an object
    if (require_object(obj) != MessageResult::Parsed) {
        return MessageResult::InvalidSchema;
    }
    // Lookup field
    auto field = obj[key];
    if (field.error()) {
        return MessageResult::Parsed; // optional, not present
    }
    // Extract boolean
    bool tmp{};
    if (field.get(tmp)) {
        return MessageResult::InvalidSchema;
    }
    out = tmp;
    return MessageResult::Parsed;
}

[[nodiscard]]
inline MessageResult parse_uint64_optional(const simdjson::dom::element& obj, const char* key, lcr::optional<std::uint64_t>& out) noexcept {
    // Always reset output (streaming safety)
    out.reset();
    // Parent must be an object
    if (require_object(obj) != MessageResult::Parsed) {
        return MessageResult::InvalidSchema;
    }
    // Lookup field
    auto field = obj[key];
    if (field.error()) {
        return MessageResult::Parsed; // optional, not present
    }
    // Extract value
    std::uint64_t tmp{};
    if (field.get(tmp)) {
        return MessageResult::InvalidSchema;
    }
    out = tmp;
    return MessageResult::Parsed;
}

[[nodiscard]]
inline MessageResult parse_double_optional(const simdjson::dom::element& obj, const char* key, lcr::optional<double>& out) noexcept {
    // Always reset output (streaming safety)
    out.reset();
    // Parent must be an object
    if (require_object(obj) != MessageResult::Parsed) {
        return MessageResult::InvalidSchema;
    }
    // Lookup field
    auto field = obj[key];
    if (field.error()) {
        return MessageResult::Parsed; // optional, not present
    }
    // Extract value
    double tmp{};
    if (field.get(tmp)) {
        return MessageResult::InvalidSchema;
    }
    out = tmp;
    return MessageResult::Parsed;
}

[[nodiscard]]
inline MessageResult parse_string_list_optional(const simdjson::dom::element& obj, const char* key, std::vector<std::string>& out, bool& present) noexcept {
    // Reset outputs
    out.clear();
    present = false;
    // Parent must be an object
    if (require_object(obj) != MessageResult::Parsed) {
        return MessageResult::InvalidSchema;
    }
    // Lookup field
    auto field = obj[key];
    if (field.error()) {
        return MessageResult::Parsed; // optional, not present
    }
    // Extract array
    simdjson::dom::array arr;
    if (field.get(arr)) {
        return MessageResult::InvalidSchema;
    }
    // Parse array elements
    for (auto v : arr) {
        std::string_view sv;
        if (v.get(sv)) {
            return MessageResult::InvalidSchema;
        }
        out.emplace_back(sv);
    }
    present = true;
    return MessageResult::Parsed;
}

} // namespace wirekrak::core::protocol::kraken::parser::dom::helper

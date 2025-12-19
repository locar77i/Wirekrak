#pragma once

#include <string_view>
#include <vector>

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


namespace wirekrak::protocol::kraken::parser::helper {

// ============================================================================
// ROOT TYPE
// ============================================================================

[[nodiscard]]
inline bool require_object(const simdjson::dom::element& root) noexcept {
    return root.type() == simdjson::dom::element_type::OBJECT;
}

// ------------------------------------------------------------
// REQUIRED OBJECT FIELD
// ------------------------------------------------------------
[[nodiscard]]
inline bool parse_object_required(const simdjson::dom::element& parent, const char* key, simdjson::dom::element& out) noexcept {
    if (!require_object(parent)) {
        return false;
    }
    // Get object field
    auto field = parent[key];
    if (field.error()) {
        return false;
    }
    // Extract object
    out = field.value();
    return require_object(out);
}

// ------------------------------------------------------------
// OPTIONAL OBJECT FIELD
// ------------------------------------------------------------
[[nodiscard]]
inline bool parse_object_optional(const simdjson::dom::element& parent, const char* key, simdjson::dom::element& out, bool& present) noexcept {
    present = false;
    if (!require_object(parent)) {
        return false;
    }
    // Get object field
    auto field = parent[key];
    if (field.error()) {
        return true; // optional, not present
    }
    // Extract object
    out = field.value();
    if (!require_object(out)) {
        return false;
    }
    present = true;
    return true;
}

// ------------------------------------------------------------
// REQUIRED ARRAY FIELD
// ------------------------------------------------------------
[[nodiscard]]
inline bool parse_array_required(const simdjson::dom::element& parent, const char* key, simdjson::dom::array& out) noexcept {
    if (!require_object(parent)) {
        return false;
    }
    // Get object field
    auto field = parent[key];
    if (field.error()) {
        return false;
    }
    // Extract array
    return !field.get(out);
}

// ------------------------------------------------------------
// OPTIONAL ARRAY FIELD
// ------------------------------------------------------------
[[nodiscard]]
inline bool parse_array_optional(const simdjson::dom::element& parent, const char* key, simdjson::dom::array& out, bool& present) noexcept {
    present = false;
    if (!require_object(parent)) {
        return false;
    }
    // Get object field
    auto field = parent[key];
    if (field.error()) {
        return true; // optional, not present
    }
    // Extract array
    if (field.get(out)) {
        return false; // wrong type
    }
    present = true;
    return true;
}


[[nodiscard]]
inline bool parse_string_equals_required(const simdjson::dom::element& obj, const char* key, std::string_view expected) noexcept {
    if (!require_object(obj)) {
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
inline bool parse_bool_required(const simdjson::dom::element& obj, const char* key, bool& out) noexcept {
    if (!require_object(obj)) {
        return false;
    }
    return !obj[key].get(out);
}

[[nodiscard]]
inline bool parse_uint64_required(const simdjson::dom::element& obj, const char* key, std::uint64_t& out) noexcept {
    if (!require_object(obj)) {
        return false;
    }
    return !obj[key].get(out);
}

[[nodiscard]]
inline bool parse_double_required(const simdjson::dom::element& obj, const char* key, double& out) noexcept {
    if (!require_object(obj)) {
        return false;
    }
    return !obj[key].get(out);
}

[[nodiscard]]
inline bool parse_string_required(const simdjson::dom::element& obj, const char* key, std::string_view& out) noexcept {
    if (!require_object(obj)) {
        return false;
    }
    return !obj[key].get(out);
}

// ============================================================================
// OPTIONAL FIELD PARSERS
// ============================================================================
[[nodiscard]]
inline bool parse_string_optional(const simdjson::dom::element& obj, const char* key, std::string_view& out) noexcept {
    if (!require_object(obj)) {
        return false;
    }
    auto field = obj[key];
    if (field.error()) {
        out = {};
        return true; // not present
    }
    if (field.get(out)) {
        return false; // wrong type
    }
    return true;
}

[[nodiscard]]
inline bool parse_bool_optional(const simdjson::dom::element& obj, const char* key, lcr::optional<bool>& out) noexcept {
    if (!require_object(obj)) {
        return false;
    }
    auto field = obj[key];
    if (field.error()) {
        return true;
    }
    bool tmp{};
    if (field.get(tmp)) {
        return false;
    }
    out = tmp;
    return true;
}

[[nodiscard]]
inline bool parse_uint64_optional(const simdjson::dom::element& obj, const char* key, lcr::optional<std::uint64_t>& out) noexcept {
    if (!require_object(obj)) {
        return false;
    }
    auto field = obj[key];
    if (field.error()) {
        return true;  // optional, not present
    }
    std::uint64_t tmp{};
    if (field.get(tmp)) {
        return false; // wrong type
    }
    out = tmp;
    return true;
}

[[nodiscard]]
inline bool parse_double_optional(const simdjson::dom::element& obj, const char* key, lcr::optional<double>& out) noexcept {
    if (!require_object(obj)) {
        return false;
    }
    auto field = obj[key];
    if (field.error()) {
        return true; // optional, not present
    }
    double tmp{};
    if (field.get(tmp)) {
        return false;
    }
    out = tmp;
    return true;
}


[[nodiscard]]
inline bool parse_string_optional(const simdjson::dom::element& obj, const char* key, lcr::optional<std::string>& out) noexcept {
    if (!require_object(obj)) {
        return false;
    }
    auto field = obj[key];
    if (field.error()) {
        return true;
    }
    std::string_view sv;
    if (field.get(sv)) {
        return false;
    }
    out = std::string(sv);
    return true;
}

[[nodiscard]]
inline bool parse_string_list_optional(const simdjson::dom::element& obj, const char* key, std::vector<std::string>& out) noexcept {
    if (!require_object(obj)) {
        return false;
    }
    auto field = obj[key];
    if (field.error()) {
        return true;  // optional, not present
    }
    simdjson::dom::array arr;
    if (field.get(arr)) {
        return false; // wrong type
    }
    for (auto v : arr) {
        std::string_view sv;
        if (v.get(sv)) {
            return false; // element not string
        }
        out.emplace_back(sv);
    }
    return true;
}

} // namespace wirekrak::protocol::kraken::parser::helper

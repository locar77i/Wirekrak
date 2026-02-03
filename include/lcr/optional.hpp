#pragma once

#include <string>
#include <sstream>
#include <type_traits>
#include <cassert>

/*
===============================================================================
 lcr::optional<T>
===============================================================================

A lightweight, non-owning "presence flag" wrapper for value types.

Unlike std::optional<T>, this type does NOT manage the lifetime of T.
The wrapped value is always constructed and destroyed with the optional
itself; the optional state only indicates whether the value is logically
present or meaningful.

Design characteristics:
- T is always constructed (requires default-constructible T)
- No placement-new, no manual lifetime management
- reset() clears logical presence but does not destroy T
- value() asserts in debug builds instead of throwing exceptions
- Deterministic layout and behavior (suitable for low-level systems code)

This type is intentionally simpler than std::optional and is designed for
infrastructure where:
- value types are cheap and trivially assignable
- exceptions are undesirable
- object lifetime boundaries are explicit and stable
- logical presence is more important than storage optimization

If you need optional lifetime semantics (non-default-constructible T,
destruction on reset, generic STL interoperability), use std::optional<T>
instead.

===============================================================================
*/

namespace lcr {

template <typename T>
class optional {
public:
    optional() : has_(false), value_{} {}
    optional(const T& v) : has_(true), value_(v) {}
    optional(T&& v) : has_(true), value_(std::move(v)) {}

    [[nodiscard]] inline bool has() const { return has_; }
    const T& value() const {
        assert(has_ && "lcr::optional::value() called when empty");
        return value_;
    }
    [[nodiscard]] inline T& value() {
        assert(has_ && "lcr::optional::value() called when empty");
        return value_;
    }
    [[nodiscard]] inline T value_or(T fallback) const {
        return has_ ? value_ : fallback;
    }

    inline void reset() {
        has_ = false;
        value_ = T{};
    }

    inline optional& operator=(const T& v) {
        value_ = v;
        has_ = true;
        return *this;
    }

    inline optional& operator=(T&& v) {
        value_ = std::move(v);
        has_ = true;
        return *this;
    }

private:
    bool has_;
    T value_;
};


template <typename T>
inline std::string to_string(const optional<T>& opt) {
    if (!opt.has()) {
        return "null";
    }
    // If T is arithmetic → use std::to_string
    if constexpr (std::is_arithmetic_v<T>) {
        return std::to_string(opt.value());
    }
    // If T is std::string or string_view
    else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>) {
        return "\"" + std::string(opt.value()) + "\"";
    }
    // Fallback: stream to string
    else {
        std::ostringstream oss;
        oss << opt.value();
        return oss.str();
    }
}

} // namespace lcr

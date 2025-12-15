#pragma once

#include <string>
#include <sstream>


namespace lcr {

template <typename T>
class optional {
public:
    optional() : has_(false), value_{} {}
    optional(const T& v) : has_(true), value_(v) {}
    optional(T&& v) : has_(true), value_(std::move(v)) {}

    bool has() const { return has_; }
    const T& value() const { return value_; }
    T& value() { return value_; }

    void reset() {
        has_ = false;
        value_ = T{};
    }

    optional& operator=(const T& v) {
        value_ = v;
        has_ = true;
        return *this;
    }

    optional& operator=(T&& v) {
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

#pragma once

#include <cstdint>

namespace lcr {

enum class time_unit {
    nanoseconds,
    microseconds,
    milliseconds,
    seconds
};

constexpr const char* to_string(time_unit unit) noexcept {
    switch (unit) {
        case time_unit::nanoseconds:  return "ns";
        case time_unit::microseconds: return "us";
        case time_unit::milliseconds: return "ms";
        case time_unit::seconds:      return "s";
        default:                     return "unknown";
    }
}

constexpr double convert_ns(uint64_t ns, time_unit unit) noexcept {
    switch (unit) {
        case time_unit::seconds:      return ns * 1e-9;
        case time_unit::milliseconds: return ns * 1e-6;
        case time_unit::microseconds: return ns * 1e-3;
        default:                     return static_cast<double>(ns);
    }
}

} // namespace lcr

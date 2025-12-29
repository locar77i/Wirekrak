#pragma once

#include <string>
#include <format>
#include <cstdint>
#include <algorithm>
#include <sstream>
#include <iomanip>


namespace lcr {

// Format a double value as a throughput string (e.g., "1.23 Mops")
inline std::string format_throughput(double value, const char* suffix = "rps") noexcept {
    static const char* units[] = {"", "K", "M", "G", "T"};
    int unit_index = 0;
    while (value >= 1000.0 && unit_index < 4) {
        value /= 1000.0;
        ++unit_index;
    }
    int precision = (value < 10.0) ? 2 : (value < 100.0) ? 1 : 0;
    return std::format("{:.{}f} {} {}", value, precision, units[unit_index], suffix);
}


// Format a duration given in nanoseconds into a human-readable string
// Examples:
//   42        -> "42 ns"
//   1'234     -> "1.23 µs"
//   12'345'678 -> "12.3 ms"
//   3'456'000'000 -> "3.46 s"
inline std::string format_duration(std::uint64_t ns) noexcept {
    double value = static_cast<double>(ns);
    const char* unit = "ns";

    if (value >= 1'000.0) {
        value /= 1'000.0;
        //unit = "µs";
        unit = "us";
    }
    if (value >= 1'000.0) {
        value /= 1'000.0;
        unit = "ms";
    }
    if (value >= 1'000.0) {
        value /= 1'000.0;
        unit = "s";
    }

    int precision =
        (value < 10.0)  ? 2 :
        (value < 100.0) ? 1 : 0;

    return std::format("{:.{}f} {}", value, precision, unit);
}


// Format a large count into a human-readable string
// Examples:
//   123        -> "123"
//   12'345     -> "12.3 K"
//   1'234'567  -> "1.23 M"
//   6'436'311  -> "6.44 M"
inline std::string format_number_scaled(uint64_t value) noexcept {
    static const char* units[] = {"", "K", "M", "B", "T"};
    double v = static_cast<double>(value);
    int unit_index = 0;

    while (v >= 1000.0 && unit_index < 4) {
        v /= 1000.0;
        ++unit_index;
    }

    int precision =
        (v < 10.0)  ? 2 :
        (v < 100.0) ? 1 : 0;

    return std::format("{:.{}f} {}", v, precision, units[unit_index]);
}


// Format an integer with thousands separators
// Example: 6436311 -> "6,436,311"
inline std::string format_number_exact(uint64_t value) {
    std::string raw = std::to_string(value);
    std::string formatted;
    formatted.reserve(raw.size() + raw.size() / 3);

    int count = 0;
    for (auto it = raw.rbegin(); it != raw.rend(); ++it) {
        if (count == 3) {
            formatted.push_back(',');
            count = 0;
        }
        formatted.push_back(*it);
        ++count;
    }
    std::reverse(formatted.begin(), formatted.end());
    return formatted;
}


// Format a number as: "<scaled> (<exact>)"
// Example: 6436311 -> "6.44 M (6,436,311)"
inline std::string format_number(uint64_t value) noexcept {
    return std::format("{} ({})",
        format_number_scaled(value),
        format_number_exact(value)
    );
}


// Format bytes as a scaled human-readable value (binary units)
// Example: 1234567 -> "1.18 MB"
inline std::string format_bytes_scaled(std::uint64_t bytes) noexcept {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit_index = 0;

    while (value >= 1024.0 && unit_index < 4) {
        value /= 1024.0;
        ++unit_index;
    }

    int precision =
        (value < 10.0)  ? 2 :
        (value < 100.0) ? 1 : 0;

    return std::format("{:.{}f} {}", value, precision, units[unit_index]);
}


// Format bytes as an exact value with thousands separators
// Example: 1234567 -> "1,234,567 bytes"
inline std::string format_bytes_exact(std::uint64_t bytes) noexcept {
    std::string raw = std::to_string(bytes);
    std::string formatted;
    formatted.reserve(raw.size() + raw.size() / 3);

    int count = 0;
    for (auto it = raw.rbegin(); it != raw.rend(); ++it) {
        if (count == 3) {
            formatted.push_back(',');
            count = 0;
        }
        formatted.push_back(*it);
        ++count;
    }
    std::reverse(formatted.begin(), formatted.end());

    return std::format("{} bytes", formatted);
}


// Format bytes as: "<scaled> (<exact>)"
// Example: 1234567 -> "1.18 MB (1,234,567 bytes)"
inline std::string format_bytes(std::uint64_t bytes) noexcept {
    return std::format("{} ({})",
        format_bytes_scaled(bytes),
        format_bytes_exact(bytes)
    );
}


} // namespace lcr

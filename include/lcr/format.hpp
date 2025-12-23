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


// Format a byte count as a human-readable string (e.g., "1.23 MB (1,234,567 bytes)")
inline std::string format_bytes(uint64_t bytes) noexcept {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes);
    int unit_index = 0;

    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        ++unit_index;
    }

    // ---- Manual comma formatting (safe version) ----
    std::string raw = std::to_string(bytes);
    std::string formatted;
    formatted.reserve(raw.size() + raw.size() / 3);  // preallocate to avoid reallocations

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

    // ---- Compose human-readable output ----
    std::ostringstream oss;
    oss << std::fixed << std::setprecision((unit_index == 0) ? 0 : 2)
        << std::setw(5) << size << ' ' << units[unit_index]
        << " (" << formatted << " bytes)";

    return oss.str();
}

} // namespace lcr

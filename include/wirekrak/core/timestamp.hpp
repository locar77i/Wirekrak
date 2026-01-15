#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <cstdlib>
#include <cstdio>
#include <cctype>

namespace wirekrak::core {

// ============================================================================
// Timestamp type
// ============================================================================
using Timestamp = std::chrono::sys_time<std::chrono::nanoseconds>;


// ============================================================================
// Helper: convert substring → integer safely
// ============================================================================
inline bool parse_int(std::string_view sv, int& out) noexcept {
    if (sv.empty()) return false;
    char* end = nullptr;
    out = std::strtol(sv.data(), &end, 10);
    return (end == sv.data() + sv.size());
}

inline bool parse_ll(std::string_view sv, long long& out) noexcept {
    if (sv.empty()) return false;
    char* end = nullptr;
    out = std::strtoll(sv.data(), &end, 10);
    return (end == sv.data() + sv.size());
}


// ============================================================================
// RFC3339 parser (example: 2023-01-02T10:22:33.123456789Z)
//
// Supports:
//   YYYY-MM-DDTHH:MM:SSZ
//   YYYY-MM-DDTHH:MM:SS.sssssssssZ
//
// Always returns Timestamp in UTC (sys_time).
// ============================================================================
[[nodiscard]] inline bool parse_rfc3339(std::string_view sv, Timestamp& out) noexcept {
    using namespace std::chrono;

    // Minimum length: "YYYY-MM-DDTHH:MM:SSZ" (20 chars)
    if (sv.size() < 20) return false;

    // ---- Parse date ----
    int year = 0, mon = 0, day = 0;

    if (!parse_int(sv.substr(0, 4), year)) return false;
    if (sv[4] != '-') return false;
    if (!parse_int(sv.substr(5, 2), mon)) return false;
    if (sv[7] != '-') return false;
    if (!parse_int(sv.substr(8, 2), day)) return false;

    // ---- Parse time ----
    if (sv[10] != 'T' && sv[10] != 't') return false;

    int hour = 0, minute = 0, sec = 0;

    if (!parse_int(sv.substr(11, 2), hour)) return false;
    if (sv[13] != ':') return false;
    if (!parse_int(sv.substr(14, 2), minute)) return false;
    if (sv[16] != ':') return false;
    if (!parse_int(sv.substr(17, 2), sec)) return false;

    // ---- Fractional seconds (optional) ----
    std::chrono::nanoseconds extra_ns{0};

    size_t pos = 19;
    if (pos < sv.size() && sv[pos] == '.') {
        size_t start = ++pos;
        while (pos < sv.size() && std::isdigit(static_cast<unsigned char>(sv[pos])))
            pos++;

        size_t digits = pos - start;
        if (digits > 0) {
            long long frac = 0;
            if (!parse_ll(sv.substr(start, digits), frac)) return false;

            if (digits < 9) {
                for (size_t i = digits; i < 9; i++)
                    frac *= 10;
            } else if (digits > 9) {
                for (size_t i = digits; i > 9; i--)
                    frac /= 10;
            }
            extra_ns = nanoseconds(frac);
        }
    }

    // ---- Time-zone: must be Z ----
    if (pos >= sv.size() || sv[pos] != 'Z') return false;

    // =====================================================================
    // Build chrono date/time — **warning-free / narrowing-safe**
    // =====================================================================
    using days = std::chrono::days;

    std::chrono::year_month_day ymd =
        std::chrono::year{year} /
        std::chrono::month{static_cast<unsigned>(mon)} /
        std::chrono::day{static_cast<unsigned>(day)};

    if (!ymd.ok()) return false;

    sys_days d{ymd};

    out = d +
          hours(hour) +
          minutes(minute) +
          seconds(sec) +
          extra_ns;

    return true;
}


// ============================================================================
// RFC3339 Formatter (always UTC)
//
// Produces:
//   YYYY-MM-DDTHH:MM:SS.sssssssssZ
// ============================================================================

[[nodiscard]] inline std::string to_string(const Timestamp& ts) {
    using namespace std::chrono;

    sys_days d = floor<days>(ts);
    year_month_day ymd{d};

    auto tod = ts - d; // time of day
    auto h = floor<hours>(tod);
    auto m = floor<minutes>(tod - h);
    auto s = floor<seconds>(tod - h - m);
    auto ns = duration_cast<nanoseconds>(tod - h - m - s).count();

    int year = int(ymd.year());
    unsigned mon = unsigned(ymd.month());
    unsigned day = unsigned(ymd.day());
    int hour = int(h.count());
    int minute = int(m.count());
    int sec = int(s.count());

    char buf[64];
    std::snprintf(buf, sizeof(buf),
                  "%04d-%02u-%02uT%02d:%02d:%02d.%09lldZ",
                  year, mon, day, hour, minute, sec,
                  static_cast<long long>(ns));

    return std::string(buf);
}

} // namespace wirekrak::core

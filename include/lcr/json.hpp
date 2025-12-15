#pragma once

#include <string>
#include <cstdint>


namespace lcr {
namespace json {


// Escape helper (minimal)
inline std::string escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        if (c == '\"') out += "\\\"";
        else out += c;
    }
    return out;
}

// Fast integer → string formatter
inline void append(std::string& out, std::uint64_t value)
{
    char buf[32];
    char* p = buf + sizeof(buf);

    do {
        *(--p) = '0' + (value % 10);
        value /= 10;
    } while (value > 0);

    out.append(p, buf + sizeof(buf) - p);
}

} // namespace json
} // namespace lcr

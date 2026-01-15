#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <sstream>

namespace wirekrak::core {

using Symbol = std::string;

inline std::string to_string(const std::vector<std::string>& symbols) {
    std::ostringstream os;
    os << '{';
    bool first = true;
    for (const auto& s : symbols) {
        if (!first) os << ", ";
        os << s;
        first = false;
    }
    os << '}';
    return os.str();
}

inline std::string to_string(const std::vector<std::string_view>& symbols) {
    std::ostringstream os;
    os << '{';
    bool first = true;
    for (auto sv : symbols) {
        if (!first) os << ", ";
        os << sv;
        first = false;
    }
    os << '}';
    return os.str();
}

} // namespace wirekrak::core

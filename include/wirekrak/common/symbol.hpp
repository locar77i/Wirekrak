#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <cassert>

#include "lcr/local/string.hpp"


namespace wirekrak {

inline constexpr std::size_t MAX_SYMBOL_LENGTH = 16;


// -----------------------------------------------------------------------------
// Utility to convert between std::string and local::string
// -----------------------------------------------------------------------------
template<std::size_t N>
lcr::local::string<N> to_local_string(const std::string& s) {
    assert(s.size() <= N);
    return lcr::local::string<N>(s);
}

// -----------------------------------------------------------------------------
// Utility to convert local::string to std::string
// -----------------------------------------------------------------------------
template<std::size_t N>
std::string to_std_string(const lcr::local::string<N>& s) {
    return std::string(s.data(), s.size());
}

// -----------------------------------------------------------------------------
// Utility to convert vector of std::string to vector of local::string
// -----------------------------------------------------------------------------
template<std::size_t N>
std::vector<lcr::local::string<N>> to_local_strings(const std::vector<std::string>& src) {
    std::vector<lcr::local::string<N>> dst;
    dst.reserve(src.size());
    for (const auto& s : src) {
        assert(s.size() <= N);
        dst.emplace_back(s);
    }
    return dst;
}

// -----------------------------------------------------------------------------
// Utility to convert vector of local::string to vector of std::string
// -----------------------------------------------------------------------------
template<std::size_t N>
std::vector<std::string> to_std_strings(const std::vector<lcr::local::string<N>>& src) {
    std::vector<std::string> dst;
    dst.reserve(src.size());
    for (const auto& s : src) {
        dst.push_back(std::string(s.data(), s.size()));
    }
    return dst;
}


// -----------------------------------------------------------------------------
// Utilities to convert symbol lists to std::string (for logging, etc.)
// -----------------------------------------------------------------------------

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


} // namespace wirekrak

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "lcr/local/string.hpp"
#include "lcr/local/vector.hpp"
#include "lcr/trap.hpp"


namespace wirekrak::core {

inline constexpr std::size_t MAX_SYMBOL_LENGTH = 16;
using Symbol = lcr::local::string<MAX_SYMBOL_LENGTH>; // max 16 chars after escaping (worst case)

inline constexpr std::size_t MAX_REQUEST_SYMBOLS = 512; // example capacity, adjust as needed
using RequestSymbols = lcr::local::vector<Symbol, MAX_REQUEST_SYMBOLS>; // example capacity, adjust as needed


// Utility to convert vector of std::string to RequestSymbols
inline RequestSymbols to_request_symbols(const std::vector<std::string>& src) {
    RequestSymbols dst;
    for (const auto& s : src) {
        LCR_ASSERT_MSG(s.size() <= RequestSymbols::max_size, "Symbol size exceeds maximum allowed length");
        dst.emplace_back(s);
    }
    return dst;
}


} // namespace wirekrak::core

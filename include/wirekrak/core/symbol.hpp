#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "wirekrak/common/symbol.hpp"
#include "lcr/local/string.hpp"
#include "lcr/local/vector.hpp"


namespace wirekrak::core {

//using Symbol = std::string;
inline constexpr std::size_t MAX_SYMBOL_LENGTH = 16;
using Symbol = lcr::local::string<MAX_SYMBOL_LENGTH>; // max 16 chars after escaping (worst case)

using Symbols = std::vector<Symbol>;
inline constexpr std::size_t MAX_SYMBOLS_PER_REQUEST = 256; // example capacity, adjust as needed
//using Symbols = lcr::local::vector<Symbol, MAX_SYMBOLS_PER_REQUEST>; // example capacity, adjust as needed



inline Symbol to_symbol(const std::string& s) {
    return wirekrak::to_local_string<MAX_SYMBOL_LENGTH>(s);
}

inline std::string to_std_string(const Symbol& s) {
    return wirekrak::to_std_string<MAX_SYMBOL_LENGTH>(s);
}


inline Symbols to_symbols(const std::vector<std::string>& src) {
    return wirekrak::to_local_strings<MAX_SYMBOL_LENGTH>(src);
}

inline std::vector<std::string> to_std_strings(const Symbols& src) {
    return wirekrak::to_std_strings<MAX_SYMBOL_LENGTH>(src);
}


} // namespace wirekrak::core

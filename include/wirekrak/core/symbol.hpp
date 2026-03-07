#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "wirekrak/common/symbol.hpp"
#include "lcr/local/string.hpp"


namespace wirekrak::core {

//using Symbol = std::string;
inline constexpr std::size_t MAX_SYMBOL_LENGTH = 16;
using Symbol = lcr::local::string<MAX_SYMBOL_LENGTH>; // max 16 chars after escaping (worst case)


inline Symbol to_symbol(const std::string& s) {
    return wirekrak::to_local_string<MAX_SYMBOL_LENGTH>(s);
}


inline std::string to_std_string(const Symbol& s) {
    return wirekrak::to_std_string<MAX_SYMBOL_LENGTH>(s);
}


inline std::vector<Symbol> to_symbols(const std::vector<std::string>& src) {
    return wirekrak::to_local_strings<MAX_SYMBOL_LENGTH>(src);
}

inline std::vector<std::string> to_std_strings(const std::vector<Symbol>& src) {
    return wirekrak::to_std_strings<MAX_SYMBOL_LENGTH>(src);
}


} // namespace wirekrak::core

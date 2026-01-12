#pragma once

#include "flashstrike/types.hpp"


namespace flashstrike {

// Invalid values
constexpr int32_t  INVALID_INDEX = -1;   // INVALID_INDEX is used as a "null pointer" for intrusive lists.
constexpr uint32_t INVALID_EVENT_ID = 0; // INVALID_EVENT_ID indicates no user (e.g., for anonymous orders).
constexpr int64_t  INVALID_PRICE = -1;   // INVALID_PRICE indicates no valid price (e.g., for best price when no levels exist).


// Bitmap constants
constexpr int BITS_PER_WORD = 64;
constexpr int WORD_SHIFT = 6;                                // log2(64)
constexpr int WORD_MASK = BITS_PER_WORD - 1;                 // 64 - 1

} // namespace flashstrike

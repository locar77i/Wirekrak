#pragma once

#include <cstdint>
#include <type_traits>
#if defined(_MSC_VER)
#  include <stdlib.h> // _byteswap_* intrinsics
#endif


// -------------------------------------------------------------
// Endian conversion helpers for WAL serialization
// -------------------------------------------------------------
// Canonical on-disk format: LITTLE-ENDIAN
// Use to_leXX() before writing to disk, from_leXX() after reading.
// -------------------------------------------------------------

// Detect endianness (portable fallback)
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)
#  define HOST_IS_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#elif defined(_WIN32)
#  define HOST_IS_LITTLE_ENDIAN 1
#else
#  error "Cannot determine host endianness"
#endif

// -------------------------------------------------------------
// Byte-swap primitives (compiler intrinsics preferred)
// -------------------------------------------------------------

#if defined(_MSC_VER)
#  include <stdlib.h>
#  define bswap16 _byteswap_ushort
#  define bswap32 _byteswap_ulong
#  define bswap64 _byteswap_uint64
#else
#  define bswap16 __builtin_bswap16
#  define bswap32 __builtin_bswap32
#  define bswap64 __builtin_bswap64
#endif


namespace lcr {

// -------------------------------------------------------------
// Conversion helpers
// -------------------------------------------------------------
inline constexpr uint16_t to_le16(uint16_t x) noexcept {
#if HOST_IS_LITTLE_ENDIAN
    return x;
#else
    return bswap16(x);
#endif
}

inline constexpr uint32_t to_le32(uint32_t x) noexcept {
#if HOST_IS_LITTLE_ENDIAN
    return x;
#else
    return bswap32(x);
#endif
}

inline constexpr uint64_t to_le64(uint64_t x) noexcept {
#if HOST_IS_LITTLE_ENDIAN
    return x;
#else
    return bswap64(x);
#endif
}

// Symmetric functions for reading back from disk
inline constexpr uint16_t from_le16(uint16_t x) noexcept { return to_le16(x); }
inline constexpr uint32_t from_le32(uint32_t x) noexcept { return to_le32(x); }
inline constexpr uint64_t from_le64(uint64_t x) noexcept { return to_le64(x); }

// -------------------------------------------------------------
// Generic template overloads (optional, for convenience)
// -------------------------------------------------------------
template <typename T>
inline constexpr T to_le(T value) noexcept {
    if constexpr (sizeof(T) == 2)
        return static_cast<T>(to_le16(static_cast<uint16_t>(value)));
    else if constexpr (sizeof(T) == 4)
        return static_cast<T>(to_le32(static_cast<uint32_t>(value)));
    else if constexpr (sizeof(T) == 8)
        return static_cast<T>(to_le64(static_cast<uint64_t>(value)));
    else
        static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8,
                      "Unsupported type size for endian conversion");
}

template <typename T>
inline constexpr T from_le(T value) noexcept {
    return to_le(value); // symmetric
}

} // namespace lcr

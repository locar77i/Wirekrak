#pragma once

#include <string>
#include <cstddef>
#include <cstdint>
#include <cstring>


namespace lcr {
namespace json {

// Fast integer → raw buffer formatter
// Returns number of characters written
inline std::size_t append(char* out, std::uint64_t value) noexcept
{
    char buf[32];
    char* p = buf + sizeof(buf);

    do {
        *(--p) = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value > 0);

    const std::size_t len = buf + sizeof(buf) - p;
    std::memcpy(out, p, len);
    return len;
}


// ============================================================================
// Zero-allocation JSON string escaper
// ----------------------------------------------------------------------------
// Writes escaped JSON string content into provided buffer.
// Does NOT add surrounding quotes.
//
// Returns number of bytes written.
//
// PRECONDITION:
//   Caller must ensure enough space is available.
//   Worst case expansion: 6 * input.size()
//   (every byte becomes \u00XX)
//
// No allocation. No exceptions. ULL-safe.
// ============================================================================

// Hex digit lookup table (branchless)
static constexpr char HEX[] = "0123456789ABCDEF";

// ----------------------------------------------------------------------------
// escape
// ----------------------------------------------------------------------------
// Escapes a UTF-8 string into JSON format.
// Handles:
//   - "  → \" 
//   - \  → \\
//   - control chars (0x00–0x1F) → \u00XX
//   - \b, \f, \n, \r, \t
//
// Does NOT validate UTF-8 (assumes valid input).
// ----------------------------------------------------------------------------

inline std::size_t escape(char* out, const char* input, std::size_t length) noexcept
{
    std::size_t pos = 0;

    for (std::size_t i = 0; i < length; ++i)
    {
        const unsigned char c = static_cast<unsigned char>(input[i]);

        switch (c)
        {
            case '\"':
                out[pos++] = '\\';
                out[pos++] = '\"';
                break;

            case '\\':
                out[pos++] = '\\';
                out[pos++] = '\\';
                break;

            case '\b':
                out[pos++] = '\\';
                out[pos++] = 'b';
                break;

            case '\f':
                out[pos++] = '\\';
                out[pos++] = 'f';
                break;

            case '\n':
                out[pos++] = '\\';
                out[pos++] = 'n';
                break;

            case '\r':
                out[pos++] = '\\';
                out[pos++] = 'r';
                break;

            case '\t':
                out[pos++] = '\\';
                out[pos++] = 't';
                break;

            default:
                if (c <= 0x1F)
                {
                    // Control characters → \u00XX
                    out[pos++] = '\\';
                    out[pos++] = 'u';
                    out[pos++] = '0';
                    out[pos++] = '0';
                    out[pos++] = HEX[(c >> 4) & 0xF];
                    out[pos++] = HEX[c & 0xF];
                }
                else
                {
                    out[pos++] = static_cast<char>(c);
                }
                break;
        }
    }

    return pos;
}

// Convenience overload for std::string-like objects
template <typename StringLike>
inline std::size_t escape(char* out, const StringLike& s) noexcept
{
    return escape(out, s.data(), s.size());
}

} // namespace json
} // namespace lcr

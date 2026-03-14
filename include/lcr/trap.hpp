#pragma once

/*
===============================================================================
lcr::trap
===============================================================================

Unified trap / assertion utilities for ultra-low-latency systems.

Design goals
------------
• Deterministic failure on invariant violation
• Assert diagnostics in debug builds
• Hard trap after assertion failure
• Zero overhead in release builds
• Header-only
• GCC / Clang friendly

Behavior
--------
Debug builds:
    LCR_ASSERT(x)      -> assert(x) + trap
    LCR_ASSERT_MSG(x)  -> assert(x) + trap
    LCR_UNREACHABLE()  -> assert(false) + trap

Release builds:
    Assertions compiled out
    UNREACHABLE hints the optimizer

===============================================================================
*/

#ifndef NDEBUG
    #include <cstdio>
#endif

namespace lcr {

// -----------------------------------------------------------------------------
// Hard trap
// -----------------------------------------------------------------------------

[[noreturn]]
inline void trap(const char* msg) noexcept {
#ifndef NDEBUG
    if (msg) {
        std::fputs("LCR_TRAP: ", stderr);
        std::fputs(msg, stderr);
        std::fputs("\n", stderr);
        std::fflush(stderr);
    }
#endif
    __builtin_trap();
}

} // namespace lcr


// ============================================================================
// RELEASE BUILD
// ============================================================================
#define LCR_STRINGIFY(x) LCR_STRINGIFY2(x)
#define LCR_STRINGIFY2(x) #x


#ifdef NDEBUG

    #define LCR_TRAP(msg) ::lcr::trap(msg " | " __FILE__ ":" LCR_STRINGIFY(__LINE__))

    #define LCR_ASSERT(expr)         ((void)0)
    #define LCR_ASSERT_MSG(expr, m)  ((void)0)

    #define LCR_UNREACHABLE()        __builtin_unreachable()


// ============================================================================
// DEBUG BUILD
// ============================================================================

#else

    #define LCR_TRAP(msg) ::lcr::trap(msg " | " __FILE__ ":" LCR_STRINGIFY(__LINE__))

    #define LCR_ASSERT(expr) \
        do { \
            if (!(expr)) [[unlikely]] { \
                LCR_TRAP("assertion failed"); \
            } \
        } while (0)

    #define LCR_ASSERT_MSG(expr, msg) \
        do { \
            if (!(expr)) [[unlikely]] { \
                LCR_TRAP(msg); \
            } \
        } while (0)

    #define LCR_UNREACHABLE() \
        do { \
            LCR_TRAP("unreachable code reached"); \
        } while (0)

#endif

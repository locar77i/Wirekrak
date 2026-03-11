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

#include <cassert>


namespace lcr {

// -----------------------------------------------------------------------------
// Hard trap
// -----------------------------------------------------------------------------

[[noreturn]]
inline void trap() noexcept {
    __builtin_trap();
}

} // namespace lcr


// ============================================================================
// RELEASE BUILD
// ============================================================================

#ifdef NDEBUG

    #define LCR_TRAP() ((void)0)

    #define LCR_ASSERT(expr)        ((void)0)
    #define LCR_ASSERT_MSG(expr, m) ((void)0)

    #define LCR_UNREACHABLE() __builtin_unreachable()


// ============================================================================
// DEBUG BUILD
// ============================================================================

#else

    #define LCR_TRAP() ::lcr::trap()

    #define LCR_ASSERT(expr) \
        do { \
            if (!(expr)) [[unlikely]] { \
                assert(expr); \
                ::lcr::trap(); \
            } \
        } while (0)

    #define LCR_ASSERT_MSG(expr, msg) \
        do { \
            if (!(expr)) [[unlikely]] { \
                assert((expr) && (msg)); \
                ::lcr::trap(); \
            } \
        } while (0)

    #define LCR_UNREACHABLE() \
        do { \
            assert(false && "unreachable"); \
            ::lcr::trap(); \
        } while (0)

#endif

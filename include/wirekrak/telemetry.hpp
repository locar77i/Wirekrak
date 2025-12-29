#pragma once

// -----------------------------------------------------------------------------
// Telemetry level configuration
// -----------------------------------------------------------------------------

#if defined(WIREKRAK_ENABLE_TELEMETRY_L1)
    #define WK_TL1(expr) expr
#else
    #define WK_TL1(expr) ((void)0)
#endif

#if defined(WIREKRAK_ENABLE_TELEMETRY_L2)
    #define WK_TL2(expr) expr
#else
    #define WK_TL2(expr) ((void)0)
#endif

#if defined(WIREKRAK_ENABLE_TELEMETRY_L3)
    #define WK_TL3(expr) expr
#else
    #define WK_TL3(expr) ((void)0)
#endif

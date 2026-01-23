#include <cassert>
#include <iostream>

// -----------------------------------------------------------------------------
// Minimal test assertion helper
// -----------------------------------------------------------------------------
#define TEST_CHECK(expr)                                                     \
    do {                                                                     \
        if (!(expr)) {                                                       \
            std::cerr << "[TEST FAILED] " << #expr                            \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort();                                                    \
        }                                                                    \
    } while (0)

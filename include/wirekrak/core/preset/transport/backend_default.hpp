#pragma once


// -----------------------------------------------------------------------------
// Backend selection
// -----------------------------------------------------------------------------

#if defined(WIREKRAK_FORCE_ASIO)
    #define WK_BACKEND_ASIO
#elif defined(WIREKRAK_FORCE_WINHTTP)
    #define WK_BACKEND_WINHTTP
#else
    #if defined(_WIN32)
        #define WK_BACKEND_WINHTTP
    #elif defined(__linux__)
        #define WK_BACKEND_ASIO
    #else
        #error "Unsupported platform"
    #endif
#endif

// -----------------------------------------------------------------------------
// Includes
// -----------------------------------------------------------------------------

#if defined(WK_BACKEND_WINHTTP)
    #include "wirekrak/core/transport/winhttp/backend.hpp"
#elif defined(WK_BACKEND_ASIO)
    #include "wirekrak/core/transport/asio/backend.hpp"
#endif


namespace wirekrak::core::preset::transport {

// -----------------------------------------------------------------------------
// Default Backend Alias
// -----------------------------------------------------------------------------

#if defined(WK_BACKEND_WINHTTP)
    using DefaultBackend = core::transport::winhttp::Backend;
#elif defined(WK_BACKEND_ASIO)
    using DefaultBackend = core::transport::asio::Backend;
#endif


} // namespace wirekrak::core::preset::transport

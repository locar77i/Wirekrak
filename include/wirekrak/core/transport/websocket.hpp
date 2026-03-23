#pragma once

#include "wirekrak/core/policy/transport/websocket_bundle.hpp"

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
    #include "wirekrak/core/transport/winhttp/websocket.hpp"
#elif defined(WK_BACKEND_ASIO)
    #include "wirekrak/core/transport/asio/websocket.hpp"
#endif

namespace wirekrak::core::transport {

// -----------------------------------------------------------------------------
// Backend namespace
// -----------------------------------------------------------------------------

#if defined(WK_BACKEND_WINHTTP)
    namespace backend = winhttp;
#elif defined(WK_BACKEND_ASIO)
    namespace backend = asio;
#endif

// -----------------------------------------------------------------------------
// Public WebSocket alias
// -----------------------------------------------------------------------------

template<
    typename ControlRing,
    typename MessageRing,
    typename PolicyBundle = policy::transport::DefaultWebsocket,
    typename Api = typename backend::RealApi
>
using WebSocket = backend::WebSocketImpl<
    ControlRing,
    MessageRing,
    PolicyBundle,
    Api
>;

} // namespace wirekrak::core::transport

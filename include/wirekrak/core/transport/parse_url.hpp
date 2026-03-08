#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "wirekrak/core/transport/error.hpp"


namespace wirekrak::core::transport {

    // Contains parsed URL components
    struct ParsedUrl {
        bool secure;                 // true = wss, false = ws
        std::string_view host;
        std::uint16_t port;
        std::string_view path;
    };


    // ---------------------------------------------------------------------
    // NOTE: Minimal URL parser supporting ws:// and wss://
    // Intentionally avoids allocations and regex.
    // This is a minimal, invariant-validated WebSocket URL parser. It accepts common
    // ws:// and wss:// URLs used by exchanges and rejects malformed inputs without
    // attempting full RFC compliance.
    //
    // Properties:
    //  • Zero allocations
    //  • Zero copies
    //  • No exceptions
    //  • Deterministic validation
    //
    // Example inputs:
    //   wss://ws.kraken.com/v2
    //   ws://example.com:8080/stream
    // ---------------------------------------------------------------------
    [[nodiscard]]
    inline Error parse_url(std::string_view url, ParsedUrl& out) noexcept {
        out = ParsedUrl{};
        // 1) Extract scheme
        constexpr std::string_view ws  = "ws://";
        constexpr std::string_view wss = "wss://";
        std::size_t pos = 0;
        if (url.starts_with(ws)) {
            out.secure = false;
            pos = ws.size();
        }
        else if (url.starts_with(wss)) {
            out.secure = true;
            pos = wss.size();
        }
        else {
            return Error::InvalidUrl;
        }
        // 2) Extract host[:port]
        std::size_t slash = url.find('/', pos);
        std::string_view hostport = (slash == std::string_view::npos) ? url.substr(pos) : url.substr(pos, slash - pos);
        if (hostport.empty()) [[unlikely]] {
            return Error::InvalidUrl;
        }
        // 3) Split host and port
        std::size_t colon = hostport.find(':');
        if (colon != std::string_view::npos) {
            out.host = hostport.substr(0, colon);
            std::string_view port_str = hostport.substr(colon + 1);
            // Validate port (invariant: must be numeric, non-zero and within valid range)
            if (port_str.empty()) [[unlikely]] {
                return Error::InvalidUrl;
            }
            std::uint32_t port = 0;
            for (char c : port_str) {
                if (c < '0' || c > '9') [[unlikely]] {
                    return Error::InvalidUrl;
                }
                port = port * 10 + (c - '0');
                if (port >= 65535) [[unlikely]] {
                    return Error::InvalidUrl;
                }
            }
            if (port == 0) [[unlikely]] {
                return Error::InvalidUrl;
            }
            out.port = static_cast<std::uint16_t>(port);
        } else {
            out.host = hostport;
            out.port = (out.secure) ? 443 : 80; // default ports
        }
        // 4) Path (default "/" if missing)
        out.path = (slash == std::string_view::npos) ? std::string_view("/") : url.substr(slash);
        // Validate path (invariant: must start with '/')
        if (out.path.empty() || out.path[0] != '/') [[unlikely]] {
            return Error::InvalidUrl;
        }

        return Error::None;
    }

} // namespace wirekrak::core::transport

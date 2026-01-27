#pragma once

#include <string>
#include <cstdlib>
#include <cstddef>

#include "wirekrak/core/transport/error.hpp"


namespace wirekrak::core::transport {

    // Contains parsed URL components
    struct ParsedUrl {
        bool secure;          // true = wss, false = ws
        std::string host;
        std::string port;
        std::string path;
    };


    // ---------------------------------------------------------------------
    // NOTE: Minimal URL parser supporting ws:// and wss://
    // Intentionally avoids allocations and regex.
    // This is a minimal, invariant-validated WebSocket URL parser. It accepts common
    // ws:// and wss:// URLs used by exchanges and rejects malformed inputs without
    // attempting full RFC compliance.
    //
    // Example inputs:
    //   wss://ws.kraken.com/v2
    //   ws://example.com:8080/stream
    // ---------------------------------------------------------------------
    [[nodiscard]]
    inline Error parse_url(const std::string& url, ParsedUrl& out) noexcept {
        out = ParsedUrl{};
        // 1) Extract scheme
        constexpr std::string_view ws  = "ws://";
        constexpr std::string_view wss = "wss://";
        size_t pos = 0;
        if (url.compare(0, ws.size(), ws) == 0) {
            out.secure = false;
            pos = ws.size();
        }
        else if (url.compare(0, wss.size(), wss) == 0) {
            out.secure = true;
            pos = wss.size();
        }
        else {
            return Error::InvalidUrl;
        }
        // 2) Extract host[:port]
        size_t slash = url.find('/', pos);
        std::string hostport = (slash == std::string::npos) ? url.substr(pos) : url.substr(pos, slash - pos);
        if (hostport.empty()) {
            return Error::InvalidUrl;
        }
        // 3) Split host and port
        size_t colon = hostport.find(':');
        if (colon != std::string::npos) {
            out.host = hostport.substr(0, colon);
            out.port = hostport.substr(colon + 1);
        } else {
            out.host = hostport;
            out.port = (out.secure) ? "443" : "80";
        }
        // 4) Path (default "/" if missing)
        out.path = (slash == std::string::npos) ? "/" : url.substr(slash);

        // Invariants check --------------------------------

        // Validate host
        if (out.host.empty() || out.port.empty()) {
            return Error::InvalidUrl;
        }
        // Validate port - must be numeric and in range
        for (char c : out.port) {
            if (c < '0' || c > '9') {
                return Error::InvalidUrl;
            }
        }
        const unsigned long p = std::strtoul(out.port.c_str(), nullptr, 10);
        if (p == 0 || p > 65535) {
            return Error::InvalidUrl;
        }
        // Validate path
        if (out.path.empty() || out.path[0] != '/') {
            return Error::InvalidUrl;
        }
        // ---------------------------------------------------

        return Error::None;
    }

} // namespace wirekrak::core::transport

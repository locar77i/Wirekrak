#pragma once

// ============================================================================
// Boost.Asio / Beast WebSocket Backend (TLS-enabled)
// ============================================================================
//
// Overview
// --------
// This backend provides a WebSocket transport implementation using
// Boost.Asio and Boost.Beast with TLS (OpenSSL).
//
// It is designed as a high-performance, low-latency backend that conforms
// to the Wirekrak BackendConcept and provides deterministic, normalized
// transport semantics on top of interruptible socket I/O.
//
// This is NOT a raw Beast wrapper. The backend enforces strict invariants
// and maps Boost/OS behavior into a stable, backend-agnostic contract.
//
// Characteristics
// ---------------
// - Blocking, synchronous API (no async state machines)
// - Zero-copy receive path (caller-provided buffers)
// - Interruptible blocking operations (via socket cancellation)
// - TLS-enabled (OpenSSL)
// - Contract-enforcing adapter (not a passthrough)
//
// Semantic Normalization
// ----------------------
// This backend normalizes Boost.Beast behavior to satisfy BackendConcept:
//
// - All CLOSE conditions (including:
//       * websocket::error::closed
//       * operation_aborted from cancellation
//   ) are mapped to:
//       { status = Ok, frame = Close }
//
// - No bytes are returned when:
//       status != Ok
//
// - Frame boundaries are explicitly reported via:
//       FrameType::{Fragment, Message, Close}
//
// - Message completion is derived from Beast's frame state and surfaced
//   directly via ReadResult (no external message_done dependency required)
//
// This ensures consistent behavior across all backends.
//
// Determinism Model
// -----------------
// This backend provides deterministic *execution semantics*:
//
// - Blocking reads are interruptible via:
//       socket.cancel()
//       socket.shutdown()
//       socket.close()
//
// - read_some() exits in bounded time after close()
//
// - No undefined states are exposed to the transport layer
//
// Therefore, this backend satisfies:
//
//      "bounded-time shutdown and interruptible receive"
//
// NOTE:
// TLS (OpenSSL) may introduce small, bounded delays during shutdown,
// but these are controlled and do not violate contract guarantees.
//
// Shutdown Semantics
// ------------------
// - close() is idempotent and thread-safe
// - A local atomic flag (`closed_`) enforces fast logical shutdown
//
// After close():
//   - read_some() returns immediately with:
//         { status = Ok, frame = Close, bytes = 0 }
//   - send() fails fast
//
// Additionally:
//   - In-flight reads are interrupted via socket cancellation
//
// Error Model
// -----------
// Boost/Asio errors are mapped into BackendConcept semantics:
//
// - websocket::error::closed
// - operation_aborted
//     → treated as graceful CLOSE
//
// - timed_out / timeout
//     → Timeout
//
// - all other errors
//     → TransportError
//
// This prioritizes semantic consistency over preserving raw error meaning.
// Higher layers (retry policy, session logic) handle recovery.
//
// Suitability
// -----------
// This backend is suitable for:
//
// ✔ Ultra-low latency (ULL) systems
// ✔ Deterministic protocol engines
// ✔ High-frequency data ingestion
// ✔ Systems requiring explicit I/O control
//
// Design Positioning
// ------------------
// This backend is the reference implementation for:
//
//   ✔ Deterministic behavior
//   ✔ Interruptible I/O
//   ✔ BackendConcept compliance
//
// Compared to WinHTTP backend:
//
//   Asio:
//     ✔ Interruptible
//     ✔ Bounded shutdown
//     ✔ ULL-suitable
//
//   WinHTTP:
//     ✘ Non-interruptible
//     ✘ Unbounded blocking
//
// Summary
// -------
// ✔ BackendConcept-compliant
// ✔ Deterministic transport semantics
// ✔ Interruptible receive (bounded shutdown)
// ✔ Zero-copy friendly
// ✔ ULL-suitable
//
// ❌ Minor TLS-induced variability (bounded)
//
// ============================================================================

#include <string>
#include <string_view>
#include <atomic>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/core/flat_buffer.hpp>

#include <openssl/ssl.h>   // REQUIRED for SNI

#include "wirekrak/core/transport/websocket/backend_concept.hpp"


namespace wirekrak::core::transport {
namespace asio {

namespace net       = boost::asio;
namespace ssl       = boost::asio::ssl;
namespace beast     = boost::beast;
namespace beast_ws  = beast::websocket;
using tcp = net::ip::tcp;

// ============================================================================
// Backend (Beast-based WebSocket wrapper, TLS-enabled)
// ============================================================================

class Backend {
public:
    Backend()
        : resolver_(ioc_)
        , ssl_ctx_(ssl::context::tls_client)
        , ws_(ioc_, ssl_ctx_)
    {
        ssl_ctx_.set_default_verify_paths();
        ssl_ctx_.set_verify_mode(ssl::verify_peer);
    }

    bool connect(std::string_view host, std::uint16_t port, std::string_view target, bool secure = true) {

        cleanup_();
        closing_.store(false, std::memory_order_release);
        closed_.store(false, std::memory_order_release);

        try {
            host_ = std::string(host);

            auto const results = resolver_.resolve(host_, std::to_string(port));

            net::connect(ws_.next_layer().next_layer(), results);

            if (secure) {
                if (!SSL_set_tlsext_host_name(
                        ws_.next_layer().native_handle(),
                        host_.c_str()))
                {
                    throw beast::system_error(
                        beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category())
                    );
                }

                ws_.next_layer().handshake(ssl::stream_base::client);
            }

            // IMPORTANT tuning
            ws_.auto_fragment(false);
            ws_.read_message_max(std::numeric_limits<std::size_t>::max());
            ws_.binary(true);

            ws_.handshake(host_, std::string(target));

            return true;
        }
        catch (const std::exception& e) {
            //std::cerr << "[ASIO] connect exception: " << e.what() << std::endl;
            cleanup_();
            return false;
        }
    }

    bool send(std::string_view msg) {
        // =========================================================
        // Preconditions
        // =========================================================
        if (closed_.load(std::memory_order_acquire)) [[unlikely]] {
            return false;
        }
        if (!ws_.is_open()) [[unlikely]] {
            return false;
        }

        // =========================================================
        // Perform send
        // =========================================================
        try {
            ws_.write(net::buffer(msg));
            return true;
        }
        catch (...) {
            return false;
        }
    }

    // ZERO-COPY RECEIVE
    websocket::ReadResult read_some(void* buffer, std::size_t size) noexcept {
        using RS = websocket::ReceiveStatus;
        using BE = websocket::BackendError;
        using FT = websocket::FrameType;

        // =========================================================
        // Preconditions
        // =========================================================
        if (closed_.load(std::memory_order_acquire)) [[unlikely]] {
            return websocket::ReadResult{
                .status = RS::Ok,
                .bytes  = 0,
                .frame  = FT::Close,
                .error  = BE::LocalShutdown,
                .native_error = 0
            };
        }

        if (!ws_.is_open()) [[unlikely]] {
            return websocket::ReadResult{
                .status = RS::Ok,
                .bytes  = 0,
                .frame  = FT::Close,
                .error = closing_.load(std::memory_order_acquire) ? BE::LocalShutdown : BE::RemoteClosed,  // RemoteClosed because in practice the socket not open anymore -> overwhelmingly remote closur
                .native_error = 0
            };
        }

        try {
            // =====================================================
            // Perform read (ZERO-COPY)
            // =====================================================
            auto buf = net::buffer(buffer, size);
            std::size_t bytes = ws_.read_some(buf);

            // =====================================================
            // Map frame boundary
            // =====================================================
            const bool done = ws_.is_message_done();

/* 
            // Note:
            // Beast can legally return bytes == 0 and done == false in some edge cases (control frames, TLS behavior)
            // So this check is too strict -> Commented out, but leaving here for reference
            if (!done) {
                // Fragment MUST have bytes > 0
                if (bytes == 0) [[unlikely]] {
                    return websocket::ReadResult{
                        .status = RS::ProtocolError,
                        .bytes  = 0,
                        .frame  = FT::Fragment,
                        .error = BE::None,
                        .native_error = 0
                    };
                }
            }
*/

            return websocket::ReadResult{
                .status = RS::Ok,
                .bytes  = bytes,
                .frame  = done ? FT::Message : FT::Fragment,
                .error = BE::None,
                .native_error = 0
            };
        }
        catch (const beast::system_error& e) {
            const auto& ec = e.code();
            return map_result_on_error_(ec);
        }
        catch (...) {
            return websocket::ReadResult{
                .status = RS::TransportError,
                .bytes  = 0,
                .frame  = FT::Fragment, // ignored
                .error = BE::TransportFailure,
                .native_error = -1
            };
        }
    }

    void close() {
        closing_.store(true, std::memory_order_release);
        // Fast path: ensure only one thread executes shutdown
        if (closed_.exchange(true, std::memory_order_acq_rel)) [[unlikely]] {
            return;
        }
        try {
            boost::system::error_code ec;
            auto& ssl  = ws_.next_layer();
            auto& sock = ssl.next_layer();
            // 1. Hard interrupt first (guarantee unblocking)
            sock.cancel(ec);
            // 2. Try graceful WS close (non-blocking-ish now)
            if (ws_.is_open()) {
                ws_.close(beast_ws::close_code::normal, ec);
            }
            // 3. TLS shutdown (best effort, now safe)
            ssl.shutdown(ec);
            // 4. Final socket teardown
            sock.shutdown(tcp::socket::shutdown_both, ec);
            sock.close(ec);
        }
        catch (...) {}
    }

    bool is_open() const noexcept {
        return !closed_.load(std::memory_order_acquire) && ws_.is_open();
    }

private:
    net::io_context ioc_;
    tcp::resolver resolver_;

    ssl::context ssl_ctx_;
    beast_ws::stream<ssl::stream<tcp::socket>> ws_;

    std::string host_;

    std::atomic<bool> closing_{false};  // intent (we initiated shutdown)
    std::atomic<bool> closed_{false};   // state (backend is shut down)

private:
    void cleanup_() noexcept {
        boost::system::error_code ec;

        auto& ssl  = ws_.next_layer();
        auto& sock = ssl.next_layer();

        sock.cancel(ec);
        ssl.shutdown(ec);   // best effort
        sock.shutdown(tcp::socket::shutdown_both, ec);
        sock.close(ec);
    }

    websocket::ReadResult map_result_on_error_(const boost::system::error_code& ec) noexcept {
        using RS = websocket::ReceiveStatus;
        using BE = websocket::BackendError;
        using FT = websocket::FrameType;

        LCR_ASSERT_MSG(ec, "map_result_on_error_ called with success code");
        // Defensive check for release builds
        if (!ec) {
            return {
                .status = RS::TransportError,
                .bytes = 0,
                .frame = FT::Fragment,
                .error = BE::TransportFailure,
                .native_error = 0
            };
        }

        // =========================================================
        // INTERRUPTED SYSTEM CALL (CTRL+C / signal / forced cancel)
        // =========================================================
        if (ec.value() == WSAEINTR) {
            return websocket::ReadResult{
                .status = RS::Ok,
                .bytes = 0,
                .frame = FT::Close,
                .error = closing_.load(std::memory_order_acquire) ? BE::LocalShutdown : BE::RemoteClosed,
                .native_error = ec.value()
            };
        }

        // =========================================
        // Graceful close (WebSocket-level)
        // =========================================
        if (ec == boost::beast::websocket::error::closed) {
            return websocket::ReadResult{
                .status = RS::Ok,
                .bytes = 0,
                .frame = FT::Close,
                .error = BE::RemoteClosed,
                .native_error = ws_.reason().code  // WS close code in this context
            };
        }

        // =========================================
        // Local shutdown
        // =========================================
        if (ec == boost::asio::error::operation_aborted) {
            return websocket::ReadResult{
                .status = RS::Ok,
                .bytes = 0,
                .frame = FT::Close,
                .error = closing_.load(std::memory_order_acquire) ? BE::LocalShutdown : BE::RemoteClosed, // RemoteClosed -> pragmatic trading system
                .native_error = ec.value()
            };
        }

        // =========================================
        // Remote closed (TCP-level)
        // =========================================
        if (ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset) {
            return websocket::ReadResult{
                .status = RS::Ok,
                .bytes = 0,
                .frame = FT::Close,
                .error = BE::RemoteClosed,
                .native_error = ec.value()
            };
        }

        // =========================================
        // Timeout
        // =========================================
        if (ec == boost::asio::error::timed_out) {
            return websocket::ReadResult{
                .status = RS::Timeout,
                .bytes = 0,
                .frame = FT::Fragment,
                .error = BE::Timeout,
                .native_error = ec.value()
            };
        }

        // =========================================
        // Fallback
        // =========================================
        return websocket::ReadResult{
            .status = RS::TransportError,
            .bytes = 0,
            .frame = FT::Fragment,
            .error = BE::TransportFailure,
            .native_error = ec.value()
        };
    }

};

} // namespace asio

// Assert BackendConcept compliance at compile-time
static_assert(websocket::BackendConcept<asio::Backend>, "asio::Backend does not satisfy websocket::BackendConcept");

} // namespace wirekrak::core::transport

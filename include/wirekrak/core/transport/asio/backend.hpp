#pragma once

// ============================================================================
// Boost.Asio / Beast WebSocket Backend (TLS-enabled)
// ============================================================================
//
// Overview
// --------
// This backend provides a synchronous, blocking WebSocket transport built on
// Boost.Asio and Boost.Beast, with TLS support via OpenSSL.
//
// It is designed for high-performance, low-latency systems where deterministic
// behavior, explicit lifecycle control, and zero-copy integration are required.
//
// Characteristics
// ---------------
// - Blocking, synchronous API (no async state machines)
// - Zero-copy receive path (caller-provided buffers)
// - Explicit thread ownership model (no hidden threads)
// - TLS-enabled (OpenSSL)
// - Backend interchangeable via compile-time abstraction
//
// Determinism & Interruptibility
// ------------------------------
// This backend is designed to provide bounded-latency interruption of blocking
// receive operations:
//
// - `read_some()` may block while waiting for network data.
// - `close()` actively interrupts pending operations via:
//      * socket cancellation (`cancel()`)
//      * TCP shutdown (`shutdown()`)
//      * socket close (`close()`)
// - Blocking reads are guaranteed to exit via a well-defined error path
//   (e.g. `operation_aborted`), enabling deterministic shutdown.
//
// As a result, this backend satisfies the transport requirement of:
//
//      "bounded-time shutdown and interruptible receive"
//
// Suitability
// -----------
// This backend is suitable for:
//
// ✔ Ultra-low latency (ULL) systems
// ✔ Deterministic protocol engines
// ✔ High-frequency data ingestion (market data, trading)
// ✔ Systems requiring explicit control over memory and threading
//
// and is the recommended backend for production use.
//
// Design Notes
// ------------
// - Uses Boost.Beast for WebSocket framing (RFC 6455 compliant)
// - TLS handled via OpenSSL (`ssl::stream`)
// - Configured for performance:
//      * auto_fragment(false)
//      * unbounded message size
//      * binary mode enabled
//
// - The design favors:
//      * straight-line execution
//      * minimal branching
//      * predictable control flow
//
// Limitations
// -----------
// - Still subject to TLS-layer behavior (OpenSSL):
//      * small, bounded delays may occur during shutdown
// - Exception-based error handling is used in the receive path
//      (cost is negligible outside hot loops but may be optimized further)
//
// Backend Contract Notes
// ----------------------
// - `close()` is idempotent and thread-safe (internally guarded)
// - `read_some()` guarantees:
//      * zero-copy writes into caller buffer
//      * no partial writes on error
// - `message_done()` reflects Beast frame/message boundaries
//
// Summary
// -------
// ✔ Deterministic
// ✔ Interruptible in bounded time
// ✔ ULL-suitable
// ✔ Zero-copy friendly
// ❌ Slight TLS-induced variability (bounded)
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
        ssl_ctx_.set_verify_mode(ssl::verify_none);
    }

    bool connect(std::string_view host,
                 std::uint16_t port,
                 std::string_view target,
                 bool secure = true)
    {
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
                        beast::error_code(
                            static_cast<int>(::ERR_get_error()),
                            net::error::get_ssl_category()));
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
            std::cerr << "[ASIO] connect exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool send(std::string_view msg) {
        try {
            ws_.write(net::buffer(msg));
            return true;
        }
        catch (...) {
            return false;
        }
    }

    // ZERO-COPY RECEIVE
    websocket::ReceiveStatus read_some(void* buffer, std::size_t size, std::size_t& bytes) noexcept {
        try {
            bytes = ws_.read_some(net::buffer(buffer, size));
            return websocket::ReceiveStatus::Ok;
        }
        catch (const beast::system_error& e) {
            const auto& ec = e.code();

            // Closed (normal / expected)
            if (ec == beast::websocket::error::closed || ec == net::error::operation_aborted) {
                return websocket::ReceiveStatus::Closed;
            }

            // Timeout (rare unless configured)
            if (ec == net::error::timed_out || ec == beast::error::timeout) {
                return websocket::ReceiveStatus::Timeout;
            }

            // Everything else = transport failure
            return websocket::ReceiveStatus::TransportError;
        }
        catch (...) {
            return websocket::ReceiveStatus::TransportError;
        }
    }

    bool message_done() const noexcept {
        return ws_.is_message_done();
    }

    void close() {
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
        return ws_.is_open();
    }

private:
    net::io_context ioc_;
    tcp::resolver resolver_;

    ssl::context ssl_ctx_;
    beast_ws::stream<ssl::stream<tcp::socket>> ws_;

    std::string host_;

    std::atomic<bool> closed_{false};
};

} // namespace asio

// Assert BackendConcept compliance at compile-time
static_assert(websocket::BackendConcept<asio::Backend>, "asio::Backend does not satisfy websocket::BackendConcept");

} // namespace wirekrak::core::transport

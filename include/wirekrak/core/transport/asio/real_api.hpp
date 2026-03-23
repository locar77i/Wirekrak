#pragma once

#include <string>
#include <string_view>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/core/flat_buffer.hpp>

#include <openssl/ssl.h>   // REQUIRED for SNI

#include "api_concept.hpp"

namespace wirekrak::core::transport::asio {

namespace net       = boost::asio;
namespace ssl       = boost::asio::ssl;
namespace beast     = boost::beast;
namespace beast_ws  = beast::websocket;
using tcp = net::ip::tcp;

// ============================================================================
// RealApi (Beast-based WebSocket wrapper, TLS-enabled)
// ============================================================================

class RealApi {
public:
    RealApi()
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
    ReceiveStatus read_some(void* buffer, std::size_t size, std::size_t& bytes) noexcept {
        try {
            bytes = ws_.read_some(net::buffer(buffer, size));
            return ReceiveStatus::Ok;
        }
        catch (const beast::system_error& e) {
            const auto& ec = e.code();

            // Closed (normal / expected)
            if (ec == beast::websocket::error::closed || ec == net::error::operation_aborted) {
                return ReceiveStatus::Closed;
            }

            // Timeout (rare unless configured)
            if (ec == net::error::timed_out || ec == beast::error::timeout) {
                return ReceiveStatus::Timeout;
            }

            // Everything else = transport failure
            return ReceiveStatus::TransportError;
        }
        catch (...) {
            return ReceiveStatus::TransportError;
        }
    }

    bool message_done() const noexcept {
        return ws_.is_message_done();
    }

    void close() {
        try {
            if (ws_.is_open()) {
                ws_.close(beast_ws::close_code::normal);
            }
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
};

// Enforce concept
static_assert(ApiConcept<RealApi>, "RealApi does not satisfy Asio ApiConcept");

} // namespace wirekrak::core::transport::asio

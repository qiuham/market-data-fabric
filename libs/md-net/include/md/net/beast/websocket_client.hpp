#pragma once

#include "md/net/websocket.hpp"

#ifndef MDF_NET_ENABLE_BEAST
#define MDF_NET_ENABLE_BEAST 1
#endif

#if MDF_NET_ENABLE_BEAST && __has_include(<boost/asio.hpp>) &&              \
    __has_include(<boost/beast/core.hpp>) &&                                \
    __has_include(<boost/beast/ssl.hpp>) &&                                 \
    __has_include(<boost/beast/websocket.hpp>) &&                           \
    __has_include(<openssl/ssl.h>)
#define MDF_NET_HAS_BEAST_WEBSOCKET 1
#else
#define MDF_NET_HAS_BEAST_WEBSOCKET 0
#endif

#if MDF_NET_HAS_BEAST_WEBSOCKET
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>
#include <openssl/ssl.h>
#endif

#include <algorithm>
#include <exception>
#include <span>
#include <string>
#include <utility>

namespace md::net::beast {

inline constexpr bool kWebSocketBackendAvailable =
    MDF_NET_HAS_BEAST_WEBSOCKET != 0;

class WebSocketClient {
public:
  explicit WebSocketClient(WebSocketClientOptions options = {})
      : options_(std::move(options)) {}

  [[nodiscard]] const WebSocketClientOptions &options() const noexcept {
    return options_;
  }

  WebSocketRunResult run(std::string_view url, WebSocketMessageHandler handler,
                         void *user_data) {
    return run(url, std::span<const std::string>{}, handler, user_data);
  }

  WebSocketRunResult run(std::string_view url,
                         std::span<const std::string> startup_messages,
                         WebSocketMessageHandler handler, void *user_data) {
    WebSocketRunResult result{};
    result.started_ns = steady_now_ns();

#if MDF_NET_HAS_BEAST_WEBSOCKET
    namespace asio = boost::asio;
    namespace beast_ns = boost::beast;
    namespace ssl = boost::asio::ssl;
    namespace websocket = boost::beast::websocket;
    using tcp = boost::asio::ip::tcp;

    WebSocketEndpoint endpoint{};
    try {
      endpoint = parse_websocket_endpoint(url);
    } catch (const std::exception &ex) {
      result.status = WebSocketRunStatus::InvalidEndpoint;
      result.error = ex.what();
      result.ended_ns = steady_now_ns();
      return result;
    }
    if (!endpoint.tls) {
      result.status = WebSocketRunStatus::InvalidEndpoint;
      result.error = "Beast backend currently supports wss:// endpoints";
      result.ended_ns = steady_now_ns();
      return result;
    }

    boost::system::error_code ec{};
    asio::io_context io_context{};
    ssl::context tls_context{ssl::context::tls_client};

    if (endpoint.tls) {
      if (options_.verify_peer) {
        tls_context.set_verify_mode(ssl::verify_peer, ec);
        if (!ec) {
          tls_context.set_default_verify_paths(ec);
        }
        if (ec) {
          result.status = WebSocketRunStatus::TlsHandshakeFailed;
          result.error = ec.message();
          result.ended_ns = steady_now_ns();
          return result;
        }
      } else {
        tls_context.set_verify_mode(ssl::verify_none, ec);
      }
    }

    websocket::stream<beast_ns::ssl_stream<beast_ns::tcp_stream>> connection{
        io_context, tls_context};
    tcp::resolver resolver{io_context};

    auto resolved = resolver.resolve(endpoint.host, endpoint.port, ec);
    if (ec) {
      result.status = WebSocketRunStatus::ResolveFailed;
      result.error = ec.message();
      result.ended_ns = steady_now_ns();
      return result;
    }

    auto &transport = beast_ns::get_lowest_layer(connection);
    transport.expires_after(options_.connect_timeout);
    transport.connect(resolved, ec);
    if (ec) {
      result.status = WebSocketRunStatus::ConnectFailed;
      result.error = ec.message();
      result.ended_ns = steady_now_ns();
      return result;
    }

    if (options_.tcp_no_delay) {
      boost::asio::ip::tcp::no_delay no_delay{true};
      transport.socket().set_option(no_delay, ec);
      ec.clear();
    }

    if (endpoint.tls) {
      if (!SSL_set_tlsext_host_name(connection.next_layer().native_handle(),
                                    endpoint.host.c_str())) {
        result.status = WebSocketRunStatus::TlsHandshakeFailed;
        result.error = "failed to set TLS SNI host name";
        result.ended_ns = steady_now_ns();
        return result;
      }

      if (options_.verify_peer) {
        connection.next_layer().set_verify_callback(
            ssl::host_name_verification(endpoint.host));
      }

      transport.expires_after(options_.handshake_timeout);
      connection.next_layer().handshake(ssl::stream_base::client, ec);
      if (ec) {
        result.status = WebSocketRunStatus::TlsHandshakeFailed;
        result.error = ec.message();
        result.ended_ns = steady_now_ns();
        return result;
      }
    }

    transport.expires_never();
    auto timeout =
        websocket::stream_base::timeout::suggested(beast_ns::role_type::client);
    timeout.handshake_timeout = options_.handshake_timeout;
    timeout.idle_timeout = options_.idle_timeout;
    timeout.keep_alive_pings = options_.keep_alive_pings;
    connection.set_option(timeout);
    connection.set_option(websocket::stream_base::decorator(
        [](websocket::request_type &request) {
          request.set(boost::beast::http::field::user_agent,
                      std::string("market-data-fabric/0.1 ") +
                          BOOST_BEAST_VERSION_STRING);
        }));

    websocket::permessage_deflate compression{};
    compression.client_enable = false;
    compression.server_enable = false;
    connection.set_option(compression);

    if (options_.max_message_bytes > 0) {
      connection.read_message_max(
          std::min(options_.max_message_bytes, kDefaultWsMessageBytes));
    }

    const auto host_header = make_host_header(endpoint);
    transport.expires_after(options_.handshake_timeout);
    connection.handshake(host_header, endpoint.target, ec);
    if (ec) {
      result.status = WebSocketRunStatus::WebSocketHandshakeFailed;
      result.error = ec.message();
      result.ended_ns = steady_now_ns();
      return result;
    }
    transport.expires_never();

    for (const auto &startup_message : startup_messages) {
      connection.write(boost::asio::buffer(startup_message), ec);
      if (ec) {
        result.status = WebSocketRunStatus::WebSocketHandshakeFailed;
        result.error = ec.message();
        result.ended_ns = steady_now_ns();
        return result;
      }
    }

    boost::beast::flat_static_buffer<kDefaultWsMessageBytes> buffer{};
    for (;;) {
      connection.read(buffer, ec);
      if (ec == websocket::error::closed) {
        result.status = WebSocketRunStatus::Completed;
        break;
      }
      if (ec) {
        result.status = WebSocketRunStatus::ReadFailed;
        result.error = ec.message();
        break;
      }

      const auto bytes = buffer.size();
      const auto readable = buffer.cdata();
      const auto *data = static_cast<const char *>(readable.data());
      WebSocketMessageView message{};
      message.recv_ts_ns = steady_now_ns();
      message.payload = std::string_view{data, bytes};
      message.binary = connection.got_binary();

      ++result.messages;
      result.bytes += static_cast<std::uint64_t>(bytes);
      const bool keep_running = handler == nullptr || handler(message, user_data);
      buffer.consume(bytes);
      if (!keep_running) {
        result.status = WebSocketRunStatus::StoppedByHandler;
        break;
      }
    }

    connection.close(websocket::close_code::normal, ec);
    if (!ec) {
      result.ended_ns = steady_now_ns();
      return result;
    }

    if (result.status == WebSocketRunStatus::Completed) {
      result.status = WebSocketRunStatus::CloseFailed;
      result.error = ec.message();
    }
    result.ended_ns = steady_now_ns();
    return result;
#else
    (void)url;
    (void)startup_messages;
    (void)handler;
    (void)user_data;
    result.status = WebSocketRunStatus::BackendUnavailable;
    result.error =
        "Boost.Beast WebSocket backend is unavailable; install Boost headers "
        "and OpenSSL headers, or build with a different md-net backend";
    result.ended_ns = steady_now_ns();
    return result;
#endif
  }

private:
  WebSocketClientOptions options_{};
};

} // namespace md::net::beast

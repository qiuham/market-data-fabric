#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace md::net {

inline constexpr std::size_t kDefaultWsMessageBytes = 1024u * 1024u;

struct WebSocketEndpoint {
  bool tls{true};
  std::string host{};
  std::string port{};
  std::string target{"/"};
};

struct WebSocketClientOptions {
  bool tcp_no_delay{true};
  bool verify_peer{true};
  std::size_t max_message_bytes{kDefaultWsMessageBytes};
  std::chrono::milliseconds connect_timeout{std::chrono::seconds(10)};
  std::chrono::milliseconds handshake_timeout{std::chrono::seconds(10)};
};

struct WebSocketMessageView {
  std::uint64_t recv_ts_ns{};
  // The payload view is valid only until the callback returns.
  std::string_view payload{};
  bool binary{false};
};

using WebSocketMessageHandler =
    bool (*)(const WebSocketMessageView &message, void *user_data) noexcept;

enum class WebSocketRunStatus : std::uint8_t {
  Completed = 0,
  StoppedByHandler = 1,
  InvalidEndpoint = 2,
  ResolveFailed = 3,
  ConnectFailed = 4,
  TlsHandshakeFailed = 5,
  WebSocketHandshakeFailed = 6,
  ReadFailed = 7,
  CloseFailed = 8,
  BackendUnavailable = 9,
};

struct WebSocketRunResult {
  WebSocketRunStatus status{WebSocketRunStatus::Completed};
  std::uint64_t messages{};
  std::uint64_t bytes{};
  std::uint64_t started_ns{};
  std::uint64_t ended_ns{};
  std::string error{};

  [[nodiscard]] bool ok() const noexcept {
    return status == WebSocketRunStatus::Completed ||
           status == WebSocketRunStatus::StoppedByHandler;
  }
};

inline std::uint64_t steady_now_ns() noexcept {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

inline WebSocketEndpoint parse_websocket_endpoint(std::string_view url) {
  constexpr std::string_view wss_prefix{"wss://"};
  constexpr std::string_view ws_prefix{"ws://"};

  WebSocketEndpoint endpoint{};
  std::string_view rest{};
  if (url.starts_with(wss_prefix)) {
    endpoint.tls = true;
    endpoint.port = "443";
    rest = url.substr(wss_prefix.size());
  } else if (url.starts_with(ws_prefix)) {
    endpoint.tls = false;
    endpoint.port = "80";
    rest = url.substr(ws_prefix.size());
  } else {
    throw std::invalid_argument("WebSocket URL must start with ws:// or wss://");
  }

  if (rest.empty()) {
    throw std::invalid_argument("WebSocket URL host must not be empty");
  }

  const auto path_pos = rest.find('/');
  const auto authority = path_pos == std::string_view::npos
                             ? rest
                             : rest.substr(0, path_pos);
  endpoint.target = path_pos == std::string_view::npos
                        ? "/"
                        : std::string(rest.substr(path_pos));
  if (authority.empty()) {
    throw std::invalid_argument("WebSocket URL host must not be empty");
  }

  const auto port_pos = authority.rfind(':');
  if (port_pos != std::string_view::npos) {
    endpoint.host = std::string(authority.substr(0, port_pos));
    endpoint.port = std::string(authority.substr(port_pos + 1));
    if (endpoint.host.empty() || endpoint.port.empty()) {
      throw std::invalid_argument("WebSocket URL host or port is empty");
    }
  } else {
    endpoint.host = std::string(authority);
  }

  return endpoint;
}

inline std::string make_host_header(const WebSocketEndpoint &endpoint) {
  const bool default_port =
      (endpoint.tls && endpoint.port == "443") ||
      (!endpoint.tls && endpoint.port == "80");
  if (default_port) {
    return endpoint.host;
  }
  return endpoint.host + ":" + endpoint.port;
}

} // namespace md::net

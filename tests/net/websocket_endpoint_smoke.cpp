#include "marketdata/net/websocket.hpp"

#include <cassert>
#include <stdexcept>

int main() {
  const auto secure = md::net::parse_websocket_endpoint(
      "wss://data-stream.binance.vision:443/ws/btcusdt@bookTicker");
  assert(secure.tls);
  assert(secure.host == "data-stream.binance.vision");
  assert(secure.port == "443");
  assert(secure.target == "/ws/btcusdt@bookTicker");
  assert(md::net::make_host_header(secure) == "data-stream.binance.vision");

  const auto local =
      md::net::parse_websocket_endpoint("ws://127.0.0.1:9001/feed");
  assert(!local.tls);
  assert(local.host == "127.0.0.1");
  assert(local.port == "9001");
  assert(local.target == "/feed");
  assert(md::net::make_host_header(local) == "127.0.0.1:9001");

  bool rejected = false;
  try {
    (void)md::net::parse_websocket_endpoint("https://example.com/ws");
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  assert(rejected);

  return 0;
}

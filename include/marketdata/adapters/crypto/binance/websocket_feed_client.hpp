#pragma once

#include "marketdata/adapters/crypto/binance/feeds.hpp"
#include "marketdata/net/beast/websocket_client.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace md::adapters::binance {

struct BinanceWebSocketFeedClientOptions {
  md::net::WebSocketClientOptions websocket{};
  std::uint64_t max_messages{};
};

struct BinanceWebSocketFeedClientRunResult {
  md::net::WebSocketRunResult websocket{};
  std::uint64_t messages{};
  std::uint64_t bytes{};
  std::uint64_t first_recv_ts_ns{};
  std::uint64_t last_recv_ts_ns{};

  [[nodiscard]] bool ok() const noexcept { return websocket.ok(); }
};

class BinanceWebSocketFeedClient {
public:
  explicit BinanceWebSocketFeedClient(BinanceConnectionSpec connection_spec,
                                      BinanceWebSocketFeedClientOptions options = {})
      : connection_spec_(std::move(connection_spec)),
        options_(std::move(options)) {}

  [[nodiscard]] const BinanceConnectionSpec &connection_spec() const noexcept {
    return connection_spec_;
  }

  [[nodiscard]] const BinanceWebSocketFeedClientOptions &options() const noexcept {
    return options_;
  }

  BinanceWebSocketFeedClientRunResult
  run(md::feed::FeedMessageHandler handler, void *user_data = nullptr) {
    if (connection_spec_.feeds.empty()) {
      BinanceWebSocketFeedClientRunResult result{};
      result.websocket.status = md::net::WebSocketRunStatus::InvalidEndpoint;
      result.websocket.started_ns = md::net::steady_now_ns();
      result.websocket.ended_ns = result.websocket.started_ns;
      result.websocket.error = "BinanceWebSocketFeedClient 至少需要一个 feed";
      return result;
    }

    CallbackState state{};
    state.connection = &connection_spec_;
    state.feed = connection_spec_.feeds.size() == 1
                     ? &connection_spec_.feeds.front()
                     : nullptr;
    state.options = &options_;
    state.handler = handler;
    state.user_data = user_data;

    md::net::beast::WebSocketClient websocket{options_.websocket};
    BinanceWebSocketFeedClientRunResult result{};
    result.websocket =
        websocket.run(connection_spec_.endpoint, connection_spec_.startup_messages,
                      &BinanceWebSocketFeedClient::on_message,
                      static_cast<void *>(&state));
    result.messages = state.messages;
    result.bytes = state.bytes;
    result.first_recv_ts_ns = state.first_recv_ts_ns;
    result.last_recv_ts_ns = state.last_recv_ts_ns;
    return result;
  }

private:
  struct CallbackState {
    const BinanceConnectionSpec *connection{};
    const BinanceResolvedFeed *feed{};
    const BinanceWebSocketFeedClientOptions *options{};
    md::feed::FeedMessageHandler handler{};
    void *user_data{};
    std::uint64_t messages{};
    std::uint64_t bytes{};
    std::uint64_t first_recv_ts_ns{};
    std::uint64_t last_recv_ts_ns{};
  };

  static bool on_message(const md::net::WebSocketMessageView &message,
                         void *user_data) noexcept {
    auto *state = static_cast<CallbackState *>(user_data);
    if (state == nullptr || state->connection == nullptr ||
        state->options == nullptr) {
      return false;
    }

    const auto capture_seq = state->messages + 1;
    auto envelope =
        state->feed == nullptr
            ? md::feed::make_connection_envelope(
                  *state->connection, capture_seq, message.recv_ts_ns)
            : md::feed::make_feed_envelope(
                  *state->connection, *state->feed, capture_seq,
                  message.recv_ts_ns);
    envelope.payload_size = static_cast<std::uint32_t>(message.payload.size());
    if (message.binary) {
      envelope.payload_encoding = md::feed::PayloadEncoding::ProviderBinary;
      envelope.flags |= md::feed::message_flag(md::feed::MessageFlag::BinaryPayload);
    }

    md::feed::FeedMessageView feed_message{};
    feed_message.envelope = envelope;
    feed_message.payload = message.payload;

    if (state->messages == 0) {
      state->first_recv_ts_ns = message.recv_ts_ns;
    }
    ++state->messages;
    state->bytes += static_cast<std::uint64_t>(message.payload.size());
    state->last_recv_ts_ns = message.recv_ts_ns;

    const bool handler_ok =
        state->handler == nullptr || state->handler(feed_message, state->user_data);
    if (!handler_ok) {
      return false;
    }
    return state->options->max_messages == 0 ||
           state->messages < state->options->max_messages;
  }

  BinanceConnectionSpec connection_spec_{};
  BinanceWebSocketFeedClientOptions options_{};
};

} // 命名空间 md::adapters::binance

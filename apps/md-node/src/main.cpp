#include "md/adapters/crypto/binance/feed_client.hpp"

#include <ctime>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using FeedKind = md::core::FeedKind;

struct BinanceLivePreviewArgs {
  std::vector<std::string> symbols{"BTCUSDT"};
  FeedKind feed_kind{FeedKind::TopOfBook};
  std::uint64_t max_messages{250};
};

[[nodiscard]] std::string_view option_value(std::string_view arg,
                                            std::string_view name) noexcept {
  if (!arg.starts_with(name)) {
    return {};
  }
  return arg.substr(name.size());
}

[[nodiscard]] bool parse_u64(std::string_view text,
                             std::uint64_t &value) noexcept {
  if (text.empty()) {
    return false;
  }
  std::uint64_t parsed = 0;
  for (char ch : text) {
    if (ch < '0' || ch > '9') {
      return false;
    }
    parsed = (parsed * 10) + static_cast<std::uint64_t>(ch - '0');
  }
  value = parsed;
  return true;
}

[[nodiscard]] bool parse_feed_kind(std::string_view text,
                                   FeedKind &feed_kind) noexcept {
  if (text == "trade") {
    feed_kind = FeedKind::Trade;
    return true;
  }
  if (text == "aggTrade") {
    feed_kind = FeedKind::AggregateTrade;
    return true;
  }
  if (text == "bookTicker") {
    feed_kind = FeedKind::TopOfBook;
    return true;
  }
  if (text == "depth") {
    feed_kind = FeedKind::BookDelta;
    return true;
  }
  if (text == "depth20") {
    feed_kind = FeedKind::BookSnapshot;
    return true;
  }
  return false;
}

[[nodiscard]] std::vector<std::string> parse_symbol_list(std::string_view text) {
  std::vector<std::string> symbols{};
  std::size_t start = 0;
  while (start <= text.size()) {
    const auto comma = text.find(',', start);
    const auto end = comma == std::string_view::npos ? text.size() : comma;
    if (end > start) {
      symbols.emplace_back(text.substr(start, end - start));
    }
    if (comma == std::string_view::npos) {
      break;
    }
    start = comma + 1;
  }
  return symbols;
}

[[nodiscard]] BinanceLivePreviewArgs parse_live_preview_args(int argc,
                                                             char **argv) {
  BinanceLivePreviewArgs args{};
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg{argv[i]};
    if (const auto value = option_value(arg, "--symbol="); !value.empty()) {
      args.symbols = {std::string(value)};
      continue;
    }
    if (const auto value = option_value(arg, "--symbols="); !value.empty()) {
      const auto symbols = parse_symbol_list(value);
      if (!symbols.empty()) {
        args.symbols = symbols;
      }
      continue;
    }
    if (const auto value = option_value(arg, "--feed="); !value.empty()) {
      FeedKind feed_kind{};
      if (parse_feed_kind(value, feed_kind)) {
        args.feed_kind = feed_kind;
      }
      continue;
    }
    if (const auto value = option_value(arg, "--messages="); !value.empty()) {
      std::uint64_t max_messages = 0;
      if (parse_u64(value, max_messages)) {
        args.max_messages = max_messages;
      }
      continue;
    }
  }
  return args;
}

[[nodiscard]] const char *status_name(
    md::net::WebSocketRunStatus status) noexcept {
  switch (status) {
  case md::net::WebSocketRunStatus::Completed:
    return "completed";
  case md::net::WebSocketRunStatus::StoppedByHandler:
    return "stopped_by_handler";
  case md::net::WebSocketRunStatus::InvalidEndpoint:
    return "invalid_endpoint";
  case md::net::WebSocketRunStatus::ResolveFailed:
    return "resolve_failed";
  case md::net::WebSocketRunStatus::ConnectFailed:
    return "connect_failed";
  case md::net::WebSocketRunStatus::TlsHandshakeFailed:
    return "tls_handshake_failed";
  case md::net::WebSocketRunStatus::WebSocketHandshakeFailed:
    return "websocket_handshake_failed";
  case md::net::WebSocketRunStatus::ReadFailed:
    return "read_failed";
  case md::net::WebSocketRunStatus::CloseFailed:
    return "close_failed";
  case md::net::WebSocketRunStatus::BackendUnavailable:
    return "backend_unavailable";
  }
  return "unknown";
}

bool count_message(
    const md::core::FeedMessageView &message, void *user_data) noexcept {
  auto *last_payload_size = static_cast<std::uint32_t *>(user_data);
  if (last_payload_size != nullptr) {
    *last_payload_size = message.envelope.payload_size;
  }
  return true;
}

} // namespace

int main(int argc, char** argv) {
    namespace binance = md::adapters::crypto::binance;

    for (int i = 1; i < argc; ++i) {
        if (std::string_view{argv[i]} == "--binance-feed-spec-preview") {
            binance::BinanceFeedSpec spec{};
            spec.market = md::core::MarketSegment::Spot;
            spec.symbols = {"BTCUSDT"};
            spec.kind = md::core::FeedKind::Trade;
            const auto connection = binance::make_connection_spec(
                1, binance::BinanceEnvironment::MarketDataOnly, spec);
            const auto &feed = connection.feeds.front();
            std::cout << "binance feed spec preview" << '\n';
            std::cout << "provider_feed_key: " << feed.provider_feed_key << '\n';
            std::cout << "endpoint: " << connection.endpoint << '\n';
            std::cout << "feed_id: " << feed.feed_id << '\n';
            return 0;
        }
        if (std::string_view{argv[i]} == "--binance-live-preview") {
            const auto args = parse_live_preview_args(argc, argv);
            binance::BinanceFeedSpec spec{};
            spec.market = md::core::MarketSegment::Spot;
            spec.symbols = args.symbols;
            spec.kind = args.feed_kind;
            if (spec.kind == md::core::FeedKind::BookSnapshot) {
                spec.depth = md::core::FeedDepth::Top20;
            }
            const auto connection = binance::make_connection_spec(
                1, binance::BinanceEnvironment::MarketDataOnly, spec);

            binance::BinanceFeedClientOptions options{};
            options.max_messages = args.max_messages;
            options.websocket.max_message_bytes = md::net::kDefaultWsMessageBytes;

            std::cout << "binance live preview" << '\n';
            std::cout << "backend: Boost.Beast";
            if (!md::net::beast::kWebSocketBackendAvailable) {
                std::cout << " (unavailable)";
            }
            std::cout << '\n';
            for (const auto &feed : connection.feeds) {
                std::cout << "provider_feed_key: " << feed.provider_feed_key << '\n';
                std::cout << "feed_id: " << feed.feed_id << '\n';
            }
            std::cout << "endpoint: " << connection.endpoint << '\n';
            std::cout << "feeds: " << connection.feeds.size() << '\n';
            std::cout << "max_messages: " << options.max_messages << '\n';

            std::uint32_t last_payload_size = 0;
            binance::BinanceFeedClient client{connection, options};
            const auto cpu_started = std::clock();
            const auto result = client.run(&count_message, &last_payload_size);
            const auto cpu_ended = std::clock();
            const auto elapsed_ns =
                result.websocket.ended_ns > result.websocket.started_ns
                    ? result.websocket.ended_ns - result.websocket.started_ns
                    : 0;
            const double elapsed_sec =
                static_cast<double>(elapsed_ns) / 1'000'000'000.0;
            const auto active_ns =
                result.last_recv_ts_ns > result.first_recv_ts_ns
                    ? result.last_recv_ts_ns - result.first_recv_ts_ns
                    : 0;
            const double active_sec =
                static_cast<double>(active_ns) / 1'000'000'000.0;
            const double msg_per_sec =
                elapsed_sec > 0.0
                    ? static_cast<double>(result.messages) / elapsed_sec
                    : 0.0;
            const double active_msg_per_sec =
                active_sec > 0.0 && result.messages > 1
                    ? static_cast<double>(result.messages - 1) / active_sec
                    : 0.0;
            const double avg_bytes =
                result.messages > 0
                    ? static_cast<double>(result.bytes) /
                          static_cast<double>(result.messages)
                    : 0.0;
            const double cpu_sec =
                static_cast<double>(cpu_ended - cpu_started) /
                static_cast<double>(CLOCKS_PER_SEC);
            const double cpu_us_per_msg =
                result.messages > 0
                    ? (cpu_sec * 1'000'000.0) /
                          static_cast<double>(result.messages)
                    : 0.0;

            std::cout << "status: " << status_name(result.websocket.status) << '\n';
            if (!result.websocket.error.empty()) {
                std::cout << "error: " << result.websocket.error << '\n';
            }
            std::cout << "messages: " << result.messages << '\n';
            std::cout << "bytes: " << result.bytes << '\n';
            std::cout << "elapsed_sec: " << elapsed_sec << '\n';
            std::cout << "msg_per_sec: " << msg_per_sec << '\n';
            std::cout << "active_sec: " << active_sec << '\n';
            std::cout << "active_msg_per_sec: " << active_msg_per_sec << '\n';
            std::cout << "cpu_sec: " << cpu_sec << '\n';
            std::cout << "cpu_us_per_msg: " << cpu_us_per_msg << '\n';
            std::cout << "avg_bytes: " << avg_bytes << '\n';
            std::cout << "last_payload_size: " << last_payload_size << '\n';
            return result.ok() ? 0 : 1;
        }
    }

    std::cout << "md-node skeleton" << '\n';
    std::cout << "roles: gateway, controller, or gateway,controller" << '\n';
    std::cout << "preview: --binance-feed-spec-preview" << '\n';
    std::cout << "live preview: --binance-live-preview "
                 "[--symbol=BTCUSDT | --symbols=BTCUSDT,ETHUSDT | --symbols=ALL] "
                 "[--feed=bookTicker] [--messages=250]" << '\n';
    std::cout << "args:";
    for (int i = 1; i < argc; ++i) {
        std::cout << ' ' << argv[i];
    }
    std::cout << '\n';
    return 0;
}

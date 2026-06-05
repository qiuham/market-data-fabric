#pragma once

#include "md/core/message.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace md::adapters::crypto::binance {

inline constexpr std::uint32_t kBinanceSpotSourceId = 110001;

enum class SpotEnvironment : std::uint8_t {
  Production = 0,
  MarketDataOnly = 1,
  Testnet = 2,
  Demo = 3,
};

enum class SpotStreamKind : std::uint8_t {
  Trade = 0,
  AggregateTrade = 1,
  BookTicker = 2,
  DiffDepth = 3,
  PartialDepth = 4,
};

enum class SpotDepthLevels : std::uint8_t {
  Five = 5,
  Ten = 10,
  Twenty = 20,
};

enum class SpotDepthSpeed : std::uint8_t {
  Default = 0,
  Ms100 = 1,
};

struct SpotSubscription {
  std::string stream_name{};
  std::string url{};
  std::uint32_t source_id{kBinanceSpotSourceId};
  std::uint32_t connection_id{};
  std::uint32_t stream_id{};
  md::core::TransportProtocol protocol{md::core::TransportProtocol::WebSocket};
  md::core::PayloadEncoding payload_encoding{
      md::core::PayloadEncoding::ProviderJson};
};

inline std::string normalize_spot_symbol(std::string_view symbol) {
  if (symbol.empty()) {
    throw std::invalid_argument("Binance spot symbol must not be empty");
  }

  std::string normalized(symbol);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char ch) {
                   if (ch >= 'A' && ch <= 'Z') {
                     return static_cast<char>(ch - 'A' + 'a');
                   }
                   return static_cast<char>(ch);
                 });
  return normalized;
}

inline std::uint32_t fnv1a32(std::string_view value) noexcept {
  std::uint32_t hash = 2166136261u;
  for (unsigned char ch : value) {
    hash ^= ch;
    hash *= 16777619u;
  }
  return hash == 0 ? 1 : hash;
}

inline std::string
make_spot_stream_name(std::string_view symbol, SpotStreamKind kind,
                      SpotDepthLevels depth_levels = SpotDepthLevels::Twenty,
                      SpotDepthSpeed depth_speed = SpotDepthSpeed::Default) {
  std::string stream = normalize_spot_symbol(symbol);
  switch (kind) {
  case SpotStreamKind::Trade:
    stream += "@trade";
    break;
  case SpotStreamKind::AggregateTrade:
    stream += "@aggTrade";
    break;
  case SpotStreamKind::BookTicker:
    stream += "@bookTicker";
    break;
  case SpotStreamKind::DiffDepth:
    stream += "@depth";
    if (depth_speed == SpotDepthSpeed::Ms100) {
      stream += "@100ms";
    }
    break;
  case SpotStreamKind::PartialDepth:
    stream += "@depth";
    stream += std::to_string(static_cast<std::uint8_t>(depth_levels));
    if (depth_speed == SpotDepthSpeed::Ms100) {
      stream += "@100ms";
    }
    break;
  }
  return stream;
}

inline std::string make_spot_ws_base_url(SpotEnvironment environment,
                                         bool combined) {
  switch (environment) {
  case SpotEnvironment::Production:
    return combined ? "wss://stream.binance.com:9443/stream?streams="
                    : "wss://stream.binance.com:9443/ws/";
  case SpotEnvironment::MarketDataOnly:
    return combined ? "wss://data-stream.binance.vision:443/stream?streams="
                    : "wss://data-stream.binance.vision:443/ws/";
  case SpotEnvironment::Testnet:
    return combined ? "wss://stream.testnet.binance.vision/stream?streams="
                    : "wss://stream.testnet.binance.vision/ws/";
  case SpotEnvironment::Demo:
    return combined ? "wss://demo-stream.binance.com:9443/stream?streams="
                    : "wss://demo-stream.binance.com:9443/ws/";
  }
  return combined ? "wss://stream.binance.com:9443/stream?streams="
                  : "wss://stream.binance.com:9443/ws/";
}

inline std::string make_spot_ws_url(SpotEnvironment environment,
                                    std::string_view stream_name) {
  return make_spot_ws_base_url(environment, false) + std::string(stream_name);
}

inline std::string
make_spot_combined_ws_url(SpotEnvironment environment,
                          const std::vector<std::string> &stream_names) {
  if (stream_names.empty()) {
    throw std::invalid_argument(
        "Binance combined stream list must not be empty");
  }

  std::string url = make_spot_ws_base_url(environment, true);
  for (std::size_t index = 0; index < stream_names.size(); ++index) {
    if (index != 0) {
      url += '/';
    }
    url += stream_names[index];
  }
  return url;
}

inline SpotSubscription make_spot_subscription(
    std::string_view symbol, SpotStreamKind kind, std::uint32_t connection_id,
    SpotEnvironment environment = SpotEnvironment::MarketDataOnly,
    SpotDepthLevels depth_levels = SpotDepthLevels::Twenty,
    SpotDepthSpeed depth_speed = SpotDepthSpeed::Default) {
  SpotSubscription subscription{};
  subscription.stream_name =
      make_spot_stream_name(symbol, kind, depth_levels, depth_speed);
  subscription.url = make_spot_ws_url(environment, subscription.stream_name);
  subscription.connection_id = connection_id;
  subscription.stream_id = fnv1a32(subscription.stream_name);
  return subscription;
}

inline md::core::MessageEnvelope
make_spot_envelope(const SpotSubscription &subscription,
                   std::uint64_t capture_seq, std::uint64_t recv_ts_ns,
                   md::core::PayloadKind payload_kind =
                       md::core::PayloadKind::ProviderMessage) noexcept {
  md::core::MessageEnvelope envelope{};
  envelope.payload_kind = payload_kind;
  envelope.payload_encoding = subscription.payload_encoding;
  envelope.source_id = subscription.source_id;
  envelope.connection_id = subscription.connection_id;
  envelope.stream_id = subscription.stream_id;
  envelope.capture_seq = capture_seq;
  envelope.recv_ts_ns = recv_ts_ns;
  envelope.protocol = subscription.protocol;
  envelope.compression = md::core::Compression::None;
  envelope.flags =
      md::core::message_flag(md::core::MessageFlag::ProviderTimestampUnknown) |
      md::core::message_flag(md::core::MessageFlag::StreamKnownWithoutParsing);
  return envelope;
}

} // namespace md::adapters::crypto::binance

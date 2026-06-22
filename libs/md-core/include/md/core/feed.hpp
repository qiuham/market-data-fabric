#pragma once

#include "md/core/message.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace md::core {

enum class VenueId : std::uint16_t {
  Unknown = 0,
  Binance = 1,
  Okx = 2,
  Bybit = 3,
  Coinbase = 4,
  Kraken = 5,
};

enum class MarketSegment : std::uint8_t {
  Unknown = 0,
  Spot = 1,
  LinearDerivative = 2,
  InverseDerivative = 3,
  Option = 4,
};

enum class FeedKind : std::uint8_t {
  Unknown = 0,
  Trade = 1,
  AggregateTrade = 2,
  TopOfBook = 3,
  BookDelta = 4,
  BookSnapshot = 5,
  Ticker = 6,
  Candle = 7,
  Status = 8,
  Heartbeat = 9,
};

enum class FeedDepth : std::uint16_t {
  Unknown = 0,
  Top1 = 1,
  Top5 = 5,
  Top10 = 10,
  Top20 = 20,
  Top50 = 50,
  Top400 = 400,
  Full = 65535,
};

enum class FeedSpeed : std::uint16_t {
  Default = 0,
  RealTime = 1,
  Ms10 = 10,
  Ms25 = 25,
  Ms50 = 50,
  Ms100 = 100,
  Ms250 = 250,
  Ms500 = 500,
  Sec1 = 1000,
  Sec5 = 5000,
};

struct FeedSpec {
  VenueId venue{VenueId::Unknown};
  MarketSegment market{MarketSegment::Unknown};
  // Empty symbols, "*" or "ALL" mean the provider's full-market feed/universe.
  std::vector<std::string> symbols{};
  FeedKind kind{FeedKind::Unknown};
  FeedDepth depth{FeedDepth::Unknown};
  FeedSpeed speed{FeedSpeed::Default};
  PayloadEncoding payload_encoding{PayloadEncoding::ProviderJson};
};

struct ResolvedFeed {
  FeedSpec spec{};
  std::string provider_symbol{};
  std::string provider_feed_key{};
  std::uint32_t source_id{};
  std::uint32_t feed_id{};
  PayloadEncoding payload_encoding{PayloadEncoding::ProviderJson};
};

struct FeedConnectionSpec {
  std::string endpoint{};
  std::uint32_t source_id{};
  std::uint32_t connection_id{};
  TransportProtocol protocol{TransportProtocol::WebSocket};
  Compression compression{Compression::None};
  std::vector<ResolvedFeed> feeds{};
  std::vector<std::string> startup_messages{};
};

struct FeedMessageView {
  MessageEnvelope envelope{};
  // The payload view is valid only until the callback returns.
  std::string_view payload{};
};

using FeedMessageHandler =
    bool (*)(const FeedMessageView &message, void *user_data) noexcept;

inline std::uint32_t stable_id32(std::string_view value) noexcept {
  std::uint32_t hash = 2166136261u;
  for (unsigned char ch : value) {
    hash ^= ch;
    hash *= 16777619u;
  }
  return hash == 0 ? 1 : hash;
}

inline bool is_all_symbol_token(std::string_view value) noexcept {
  if (value == "*") {
    return true;
  }
  if (value.size() != 3) {
    return false;
  }
  const auto upper = [](char ch) noexcept {
    return ch >= 'a' && ch <= 'z' ? static_cast<char>(ch - 'a' + 'A') : ch;
  };
  return upper(value[0]) == 'A' && upper(value[1]) == 'L' &&
         upper(value[2]) == 'L';
}

inline bool is_all_symbols(const std::vector<std::string> &symbols) noexcept {
  return symbols.empty() ||
         (symbols.size() == 1 && is_all_symbol_token(symbols.front()));
}

inline MessageEnvelope make_feed_envelope(
    const FeedConnectionSpec &connection, const ResolvedFeed &feed,
    std::uint64_t capture_seq, std::uint64_t recv_ts_ns,
    PayloadKind payload_kind = PayloadKind::ProviderMessage) noexcept {
  MessageEnvelope envelope{};
  envelope.payload_kind = payload_kind;
  envelope.payload_encoding = feed.payload_encoding;
  envelope.source_id =
      feed.source_id == 0 ? connection.source_id : feed.source_id;
  envelope.connection_id = connection.connection_id;
  envelope.feed_id = feed.feed_id;
  envelope.capture_seq = capture_seq;
  envelope.recv_ts_ns = recv_ts_ns;
  envelope.protocol = connection.protocol;
  envelope.compression = connection.compression;
  envelope.flags =
      message_flag(MessageFlag::ProviderTimestampUnknown) |
      message_flag(MessageFlag::FeedKnownWithoutParsing);
  return envelope;
}

inline MessageEnvelope make_connection_envelope(
    const FeedConnectionSpec &connection, std::uint64_t capture_seq,
    std::uint64_t recv_ts_ns,
    PayloadKind payload_kind = PayloadKind::ProviderMessage,
    PayloadEncoding payload_encoding = PayloadEncoding::ProviderJson) noexcept {
  MessageEnvelope envelope{};
  envelope.payload_kind = payload_kind;
  envelope.payload_encoding = payload_encoding;
  envelope.source_id = connection.source_id;
  envelope.connection_id = connection.connection_id;
  envelope.capture_seq = capture_seq;
  envelope.recv_ts_ns = recv_ts_ns;
  envelope.protocol = connection.protocol;
  envelope.compression = connection.compression;
  envelope.flags = message_flag(MessageFlag::ProviderTimestampUnknown);
  return envelope;
}

} // namespace md::core

#pragma once

#include "md/core/feed.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace md::adapters::crypto::binance {

inline constexpr std::uint32_t kBinanceSourceId = 110001;

enum class BinanceEnvironment : std::uint8_t {
  Production = 0,
  MarketDataOnly = 1,
  Testnet = 2,
  Demo = 3,
};

struct BinanceFeedSpec {
  md::core::MarketSegment market{md::core::MarketSegment::Spot};
  // Empty, "*" or "ALL" means the provider's full-market feed/universe.
  std::vector<std::string> symbols{};
  md::core::FeedKind kind{md::core::FeedKind::Trade};
  md::core::FeedDepth depth{md::core::FeedDepth::Unknown};
  md::core::FeedSpeed speed{md::core::FeedSpeed::Default};
  md::core::PayloadEncoding payload_encoding{
      md::core::PayloadEncoding::ProviderJson};
};

using BinanceResolvedFeed = md::core::ResolvedFeed;
using BinanceConnectionSpec = md::core::FeedConnectionSpec;

inline std::string normalize_symbol(std::string_view symbol) {
  if (symbol.empty()) {
    throw std::invalid_argument("Binance symbol must not be empty");
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

inline std::string make_feed_id_seed(
    md::core::MarketSegment market, std::string_view provider_feed_key) {
  return "binance:" + std::to_string(static_cast<std::uint8_t>(market)) + ":" +
         std::string(provider_feed_key);
}

inline void require_supported_market(md::core::MarketSegment market) {
  if (market != md::core::MarketSegment::Spot &&
      market != md::core::MarketSegment::LinearDerivative &&
      market != md::core::MarketSegment::InverseDerivative &&
      market != md::core::MarketSegment::Option) {
    throw std::invalid_argument("Unsupported Binance market segment");
  }
}

inline std::string speed_suffix(md::core::FeedSpeed speed) {
  switch (speed) {
  case md::core::FeedSpeed::Default:
  case md::core::FeedSpeed::RealTime:
    return {};
  case md::core::FeedSpeed::Ms100:
    return "@100ms";
  case md::core::FeedSpeed::Ms250:
    return "@250ms";
  case md::core::FeedSpeed::Ms500:
    return "@500ms";
  default:
    throw std::invalid_argument("Unsupported Binance feed speed");
  }
}

inline std::uint16_t depth_levels(md::core::FeedDepth depth) {
  switch (depth) {
  case md::core::FeedDepth::Top5:
  case md::core::FeedDepth::Top10:
  case md::core::FeedDepth::Top20:
    return static_cast<std::uint16_t>(depth);
  default:
    throw std::invalid_argument("Unsupported Binance partial book depth");
  }
}

inline std::string make_provider_feed_key(const BinanceFeedSpec &spec,
                                          std::string_view symbol) {
  require_supported_market(spec.market);

  std::string key = normalize_symbol(symbol);
  switch (spec.kind) {
  case md::core::FeedKind::Trade:
    key += spec.market == md::core::MarketSegment::Option ? "@optionTrade"
                                                          : "@trade";
    break;
  case md::core::FeedKind::AggregateTrade:
    key += "@aggTrade";
    break;
  case md::core::FeedKind::TopOfBook:
    key += "@bookTicker";
    break;
  case md::core::FeedKind::BookUpdate:
    key += "@depth";
    key += speed_suffix(spec.speed);
    break;
  case md::core::FeedKind::BookSnapshot:
    key += "@depth";
    key += std::to_string(depth_levels(spec.depth));
    key += speed_suffix(spec.speed);
    break;
  default:
    throw std::invalid_argument("Unsupported Binance feed kind");
  }
  return key;
}

inline std::string make_all_symbols_provider_feed_key(
    const BinanceFeedSpec &spec) {
  require_supported_market(spec.market);

  if (spec.kind == md::core::FeedKind::TopOfBook) {
    return "!bookTicker";
  }

  throw std::invalid_argument(
      "Binance all-symbol feed currently requires a provider aggregate feed; "
      "use TopOfBook or expand the universe before resolving this feed kind");
}

inline BinanceResolvedFeed resolve_symbol_feed(const BinanceFeedSpec &spec,
                                               std::string_view symbol) {
  BinanceResolvedFeed feed{};
  feed.spec.venue = md::core::VenueId::Binance;
  feed.spec.market = spec.market;
  feed.spec.symbols = {std::string(symbol)};
  feed.spec.kind = spec.kind;
  feed.spec.depth = spec.depth;
  feed.spec.speed = spec.speed;
  feed.spec.payload_encoding = spec.payload_encoding;
  feed.provider_symbol = normalize_symbol(symbol);
  feed.provider_feed_key = make_provider_feed_key(spec, symbol);
  feed.source_id = kBinanceSourceId;
  feed.feed_id = md::core::stable_id32(
      make_feed_id_seed(spec.market, feed.provider_feed_key));
  feed.payload_encoding = spec.payload_encoding;
  return feed;
}

inline BinanceResolvedFeed resolve_all_symbols_feed(
    const BinanceFeedSpec &spec) {
  BinanceResolvedFeed feed{};
  feed.spec.venue = md::core::VenueId::Binance;
  feed.spec.market = spec.market;
  feed.spec.symbols = {};
  feed.spec.kind = spec.kind;
  feed.spec.depth = spec.depth;
  feed.spec.speed = spec.speed;
  feed.spec.payload_encoding = spec.payload_encoding;
  feed.provider_feed_key = make_all_symbols_provider_feed_key(spec);
  feed.source_id = kBinanceSourceId;
  feed.feed_id = md::core::stable_id32(
      make_feed_id_seed(spec.market, feed.provider_feed_key));
  feed.payload_encoding = spec.payload_encoding;
  return feed;
}

inline std::vector<BinanceResolvedFeed> resolve_feeds(
    const BinanceFeedSpec &spec) {
  if (md::core::is_all_symbols(spec.symbols)) {
    return {resolve_all_symbols_feed(spec)};
  }

  std::vector<BinanceResolvedFeed> feeds{};
  feeds.reserve(spec.symbols.size());
  for (const auto &symbol : spec.symbols) {
    if (md::core::is_all_symbol_token(symbol)) {
      throw std::invalid_argument(
          "Binance ALL symbol token must be the only symbol in the list");
    }
    feeds.push_back(resolve_symbol_feed(spec, symbol));
  }
  return feeds;
}

inline BinanceResolvedFeed resolve_feed(const BinanceFeedSpec &spec) {
  const auto feeds = resolve_feeds(spec);
  if (feeds.size() != 1) {
    throw std::invalid_argument(
        "resolve_feed requires exactly one resolved Binance feed");
  }
  return feeds.front();
}

inline std::string make_endpoint_base(md::core::MarketSegment market,
                                      BinanceEnvironment environment,
                                      bool multi_feed) {
  require_supported_market(market);

  if (market == md::core::MarketSegment::Spot) {
    switch (environment) {
    case BinanceEnvironment::Production:
      return multi_feed ? "wss://stream.binance.com:9443/stream?streams="
                        : "wss://stream.binance.com:9443/ws/";
    case BinanceEnvironment::MarketDataOnly:
      return multi_feed
                 ? "wss://data-stream.binance.vision:443/stream?streams="
                 : "wss://data-stream.binance.vision:443/ws/";
    case BinanceEnvironment::Testnet:
      return multi_feed ? "wss://stream.testnet.binance.vision/stream?streams="
                        : "wss://stream.testnet.binance.vision/ws/";
    case BinanceEnvironment::Demo:
      return multi_feed ? "wss://demo-stream.binance.com:9443/stream?streams="
                        : "wss://demo-stream.binance.com:9443/ws/";
    }
  }

  if (environment == BinanceEnvironment::Testnet ||
      environment == BinanceEnvironment::Demo) {
    throw std::invalid_argument(
        "Binance non-spot non-production endpoints are not configured");
  }

  if (market == md::core::MarketSegment::LinearDerivative) {
    return multi_feed ? "wss://fstream.binance.com/stream?streams="
                      : "wss://fstream.binance.com/ws/";
  }
  if (market == md::core::MarketSegment::InverseDerivative) {
    return multi_feed ? "wss://dstream.binance.com/stream?streams="
                      : "wss://dstream.binance.com/ws/";
  }
  if (market == md::core::MarketSegment::Option) {
    return multi_feed ? "wss://fstream.binance.com/public/stream?streams="
                      : "wss://fstream.binance.com/public/ws/";
  }

  throw std::invalid_argument("Unsupported Binance market endpoint");
}

inline BinanceConnectionSpec make_connection_spec(
    std::uint32_t connection_id, BinanceEnvironment environment,
    const std::vector<BinanceResolvedFeed> &feeds) {
  if (feeds.empty()) {
    throw std::invalid_argument("Binance feed list must not be empty");
  }

  const auto market = feeds.front().spec.market;
  for (const auto &feed : feeds) {
    if (feed.spec.market != market) {
      throw std::invalid_argument(
          "Binance feeds in one connection must use the same market segment");
    }
  }

  BinanceConnectionSpec connection{};
  connection.source_id = kBinanceSourceId;
  connection.connection_id = connection_id;
  connection.protocol = md::core::TransportProtocol::WebSocket;
  connection.compression = md::core::Compression::None;
  connection.feeds = feeds;

  if (feeds.size() == 1) {
    connection.endpoint =
        make_endpoint_base(market, environment, false) + feeds[0].provider_feed_key;
    return connection;
  }

  connection.endpoint = make_endpoint_base(market, environment, true);
  for (std::size_t index = 0; index < feeds.size(); ++index) {
    if (index != 0) {
      connection.endpoint += '/';
    }
    connection.endpoint += feeds[index].provider_feed_key;
  }
  return connection;
}

inline BinanceConnectionSpec make_connection_spec(
    std::uint32_t connection_id, BinanceEnvironment environment,
    const BinanceFeedSpec &spec) {
  return make_connection_spec(connection_id, environment, resolve_feeds(spec));
}

} // namespace md::adapters::crypto::binance

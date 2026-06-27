#pragma once

#include "md/adapters/crypto/binance/endpoints.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace md::adapters::crypto::binance {

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
          "Binance ALL symbol token 必须是列表里唯一的 symbol");
    }
    feeds.push_back(resolve_symbol_feed(spec, symbol));
  }
  return feeds;
}

inline BinanceResolvedFeed resolve_feed(const BinanceFeedSpec &spec) {
  const auto feeds = resolve_feeds(spec);
  if (feeds.size() != 1) {
    throw std::invalid_argument("resolve_feed 要求只有一个已解析的 Binance feed");
  }
  return feeds.front();
}

inline BinanceConnectionSpec make_connection_spec(
    std::uint32_t connection_id, BinanceEnvironment environment,
    const std::vector<BinanceResolvedFeed> &feeds) {
  if (feeds.empty()) {
    throw std::invalid_argument("Binance feed 列表不能为空");
  }

  const auto market = feeds.front().spec.market;
  for (const auto &feed : feeds) {
    if (feed.spec.market != market) {
      throw std::invalid_argument(
          "同一个 Binance connection 内的 feeds 必须使用相同 market segment");
    }
  }

  BinanceConnectionSpec connection{};
  connection.source_id = kBinanceSourceId;
  connection.connection_id = connection_id;
  connection.protocol = md::core::TransportProtocol::WebSocket;
  connection.compression = md::core::Compression::None;
  connection.feeds = feeds;

  if (feeds.size() == 1) {
    connection.endpoint = make_endpoint_base(market, environment, false) +
                          feeds[0].provider_feed_key;
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

} // 命名空间 md::adapters::crypto::binance

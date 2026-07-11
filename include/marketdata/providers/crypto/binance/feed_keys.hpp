#pragma once

#include "marketdata/providers/crypto/binance/symbols.hpp"
#include "marketdata/providers/crypto/binance/types.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace md::providers::binance {

inline std::string make_feed_id_seed(
    md::feed::MarketSegment market, std::string_view provider_feed_key) {
  return "binance:" + std::to_string(static_cast<std::uint8_t>(market)) + ":" +
         std::string(provider_feed_key);
}

inline void require_supported_market(md::feed::MarketSegment market) {
  if (market != md::feed::MarketSegment::Spot &&
      market != md::feed::MarketSegment::LinearDerivative &&
      market != md::feed::MarketSegment::InverseDerivative &&
      market != md::feed::MarketSegment::Option) {
    throw std::invalid_argument("不支持的 Binance market segment");
  }
}

inline std::string speed_suffix(md::feed::FeedSpeed speed) {
  switch (speed) {
  case md::feed::FeedSpeed::Default:
  case md::feed::FeedSpeed::RealTime:
    return {};
  case md::feed::FeedSpeed::Ms100:
    return "@100ms";
  case md::feed::FeedSpeed::Ms250:
    return "@250ms";
  case md::feed::FeedSpeed::Ms500:
    return "@500ms";
  default:
    throw std::invalid_argument("不支持的 Binance feed speed");
  }
}

inline std::uint16_t depth_levels(md::feed::FeedDepth depth) {
  switch (depth) {
  case md::feed::FeedDepth::Top5:
  case md::feed::FeedDepth::Top10:
  case md::feed::FeedDepth::Top20:
    return static_cast<std::uint16_t>(depth);
  default:
    throw std::invalid_argument("不支持的 Binance partial book depth");
  }
}

inline std::string make_provider_feed_key(const BinanceFeedSpec &spec,
                                          std::string_view symbol) {
  require_supported_market(spec.market);

  std::string key = normalize_symbol(symbol);
  switch (spec.kind) {
  case md::feed::FeedKind::Trade:
    key += spec.market == md::feed::MarketSegment::Option ? "@optionTrade"
                                                          : "@trade";
    break;
  case md::feed::FeedKind::AggregateTrade:
    key += "@aggTrade";
    break;
  case md::feed::FeedKind::TopOfBook:
    key += "@bookTicker";
    break;
  case md::feed::FeedKind::BookUpdate:
    key += "@depth";
    key += speed_suffix(spec.speed);
    break;
  case md::feed::FeedKind::BookSnapshot:
    key += "@depth";
    key += std::to_string(depth_levels(spec.depth));
    key += speed_suffix(spec.speed);
    break;
  default:
    throw std::invalid_argument("不支持的 Binance feed kind");
  }
  return key;
}

inline std::string make_all_symbols_provider_feed_key(
    const BinanceFeedSpec &spec) {
  require_supported_market(spec.market);

  if (spec.kind == md::feed::FeedKind::TopOfBook) {
    return "!bookTicker";
  }

  throw std::invalid_argument(
      "Binance 全市场 feed 当前要求 provider 支持聚合 feed；"
      "请使用 TopOfBook，或在解析 feed kind 前展开 universe");
}

} // 命名空间 md::providers::binance

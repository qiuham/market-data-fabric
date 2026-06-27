#pragma once

#include "md/adapters/crypto/binance/symbols.hpp"
#include "md/adapters/crypto/binance/types.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace md::adapters::crypto::binance {

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
    throw std::invalid_argument("不支持的 Binance market segment");
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
    throw std::invalid_argument("不支持的 Binance feed speed");
  }
}

inline std::uint16_t depth_levels(md::core::FeedDepth depth) {
  switch (depth) {
  case md::core::FeedDepth::Top5:
  case md::core::FeedDepth::Top10:
  case md::core::FeedDepth::Top20:
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
    throw std::invalid_argument("不支持的 Binance feed kind");
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
      "Binance 全市场 feed 当前要求 provider 支持聚合 feed；"
      "请使用 TopOfBook，或在解析 feed kind 前展开 universe");
}

} // 命名空间 md::adapters::crypto::binance

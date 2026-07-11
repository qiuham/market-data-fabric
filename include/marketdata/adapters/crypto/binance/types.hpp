#pragma once

#include "marketdata/feed/feed.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace md::adapters::binance {

inline constexpr std::uint32_t kBinanceSourceId = 110001;

enum class BinanceEnvironment : std::uint8_t {
  Production = 0,
  MarketDataOnly = 1,
  Testnet = 2,
  Demo = 3,
};

struct BinanceFeedSpec {
  md::feed::MarketSegment market{md::feed::MarketSegment::Spot};
  // 空、"*" 或 "ALL" 表示 provider 的全市场 feed/universe。
  std::vector<std::string> symbols{};
  md::feed::FeedKind kind{md::feed::FeedKind::Trade};
  md::feed::FeedDepth depth{md::feed::FeedDepth::Unknown};
  md::feed::FeedSpeed speed{md::feed::FeedSpeed::Default};
  md::feed::PayloadEncoding payload_encoding{
      md::feed::PayloadEncoding::ProviderJson};
};

using BinanceResolvedFeed = md::feed::ResolvedFeed;
using BinanceConnectionSpec = md::feed::FeedConnectionSpec;

} // 命名空间 md::adapters::binance

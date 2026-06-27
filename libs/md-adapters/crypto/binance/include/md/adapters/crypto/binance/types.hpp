#pragma once

#include "md/core/feed.hpp"

#include <cstdint>
#include <string>
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
  // 空、"*" 或 "ALL" 表示 provider 的全市场 feed/universe。
  std::vector<std::string> symbols{};
  md::core::FeedKind kind{md::core::FeedKind::Trade};
  md::core::FeedDepth depth{md::core::FeedDepth::Unknown};
  md::core::FeedSpeed speed{md::core::FeedSpeed::Default};
  md::core::PayloadEncoding payload_encoding{
      md::core::PayloadEncoding::ProviderJson};
};

using BinanceResolvedFeed = md::core::ResolvedFeed;
using BinanceConnectionSpec = md::core::FeedConnectionSpec;

} // 命名空间 md::adapters::crypto::binance

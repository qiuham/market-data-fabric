#pragma once

#include "marketdata/providers/crypto/binance/feed_keys.hpp"

#include <stdexcept>
#include <string>

namespace md::providers::binance {

inline std::string make_endpoint_base(md::feed::MarketSegment market,
                                      BinanceEnvironment environment,
                                      bool multi_feed) {
  require_supported_market(market);

  if (market == md::feed::MarketSegment::Spot) {
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
    throw std::invalid_argument("Binance 非 spot 的非生产 endpoint 尚未配置");
  }

  if (market == md::feed::MarketSegment::LinearDerivative) {
    return multi_feed ? "wss://fstream.binance.com/stream?streams="
                      : "wss://fstream.binance.com/ws/";
  }
  if (market == md::feed::MarketSegment::InverseDerivative) {
    return multi_feed ? "wss://dstream.binance.com/stream?streams="
                      : "wss://dstream.binance.com/ws/";
  }
  if (market == md::feed::MarketSegment::Option) {
    return multi_feed ? "wss://fstream.binance.com/public/stream?streams="
                      : "wss://fstream.binance.com/public/ws/";
  }

  throw std::invalid_argument("不支持的 Binance market endpoint");
}

} // 命名空间 md::providers::binance

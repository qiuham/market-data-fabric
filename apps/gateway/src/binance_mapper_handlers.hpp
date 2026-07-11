#pragma once

#include "marketdata/providers/crypto/binance/book_ticker_mapper.hpp"
#include "marketdata/providers/crypto/binance/trade_mapper.hpp"
#include "marketdata/feed/feed.hpp"
#include "marketdata/service/mapper_stats.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace md::apps::md_node {

[[nodiscard]] std::uint32_t
make_binance_spot_instrument_id(std::string_view provider_symbol);

struct BinanceQuoteMapperHandler {
  const md::providers::binance::BinanceBookTickerMappingContext
      *context{};
  md::service::MapperRuntimeStats *stats{};
  std::uint64_t max_logged_events{};
  std::uint64_t logged_events{};
  bool log_events{};

  static bool handle(const md::feed::FeedMessageView &message,
                     void *user_data) noexcept;
  bool on_message(const md::feed::FeedMessageView &message) noexcept;
};

struct BinanceTradeMapperHandler {
  const md::providers::binance::BinanceTradeMappingContext *context{};
  md::service::MapperRuntimeStats *stats{};
  std::uint64_t max_logged_events{};
  std::uint64_t logged_events{};
  bool log_events{};

  static bool handle(const md::feed::FeedMessageView &message,
                     void *user_data) noexcept;
  bool on_message(const md::feed::FeedMessageView &message) noexcept;
};

} // 命名空间 md::apps::md_node

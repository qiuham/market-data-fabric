#pragma once

#include "md/adapters/crypto/binance/book_ticker_mapper.hpp"
#include "md/adapters/crypto/binance/trade_mapper.hpp"
#include "md/core/feed.hpp"
#include "md/service/mapper_stats.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace md::apps::md_node {

[[nodiscard]] std::uint32_t
make_binance_spot_instrument_id(std::string_view provider_symbol);

struct BinanceQuoteMapperHandler {
  const md::adapters::crypto::binance::BinanceBookTickerMappingContext
      *context{};
  md::service::MapperRuntimeStats *stats{};
  std::uint64_t max_logged_events{};
  std::uint64_t logged_events{};
  bool log_events{};

  static bool handle(const md::core::FeedMessageView &message,
                     void *user_data) noexcept;
  bool on_message(const md::core::FeedMessageView &message) noexcept;
};

struct BinanceTradeMapperHandler {
  const md::adapters::crypto::binance::BinanceTradeMappingContext *context{};
  md::service::MapperRuntimeStats *stats{};
  std::uint64_t max_logged_events{};
  std::uint64_t logged_events{};
  bool log_events{};

  static bool handle(const md::core::FeedMessageView &message,
                     void *user_data) noexcept;
  bool on_message(const md::core::FeedMessageView &message) noexcept;
};

} // 命名空间 md::apps::md_node

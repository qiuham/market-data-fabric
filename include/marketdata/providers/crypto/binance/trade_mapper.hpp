#pragma once

#include "marketdata/providers/crypto/binance/detail/json_value_reader.hpp"
#include "marketdata/providers/crypto/binance/mapping_context.hpp"
#include "marketdata/mappers/event_header.hpp"
#include "marketdata/codecs/fixed_point.hpp"
#include "trading/events/market.hpp"

#include <limits>
#include <string_view>

namespace md::providers::binance {

[[nodiscard]] inline bool ms_to_ns(std::uint64_t ms,
                                   std::uint64_t &ns) noexcept {
  if (ms > std::numeric_limits<std::uint64_t>::max() / 1'000'000u) {
    return false;
  }
  ns = ms * 1'000'000u;
  return true;
}

[[nodiscard]] inline bool map_trade_to_trade(
    std::string_view payload, const BinanceTradeMappingContext &context,
    trading::events::Trade &out) noexcept {
  std::string_view symbol{};
  std::string_view price_text{};
  std::string_view quantity_text{};
  std::uint64_t trade_id = 0;
  std::uint64_t trade_time_ms = 0;
  bool buyer_is_maker = false;

  if (!detail::read_json_string(payload, "s", symbol) ||
      !detail::read_json_u64(payload, "t", trade_id) ||
      !detail::read_json_string(payload, "p", price_text) ||
      !detail::read_json_string(payload, "q", quantity_text) ||
      !detail::read_json_u64(payload, "T", trade_time_ms) ||
      !detail::read_json_bool(payload, "m", buyer_is_maker)) {
    return false;
  }

  if (!context.provider_symbol.empty() &&
      !detail::ascii_equal_ignore_case(symbol, context.provider_symbol)) {
    return false;
  }

  std::int64_t price = 0;
  std::int64_t quantity = 0;
  std::uint64_t source_ts_ns = 0;
  if (!md::codecs::parse_decimal_to_fixed(price_text, context.price_scale,
                                        price) ||
      !md::codecs::parse_decimal_to_fixed(quantity_text, context.quantity_scale,
                                        quantity) ||
      !ms_to_ns(trade_time_ms, source_ts_ns)) {
    return false;
  }

  trading::events::Trade trade{};
  trade.header.source_id = context.source_id;
  trade.header.venue_id = context.venue_id;
  trade.header.instrument_id = context.instrument_id;
  trade.header.exchange_seq = trade_id;
  trade.header.exchange_ts_ns = source_ts_ns;
  trade.price = price;
  trade.quantity = quantity;
  trade.trade_id = trade_id;
  trade.aggressor_side = buyer_is_maker
                             ? trading::core::AggressorSide::Sell
                             : trading::core::AggressorSide::Buy;
  out = trade;
  return true;
}

[[nodiscard]] inline bool map_trade_to_trade(
    const md::feed::FeedMessageView &message,
    const BinanceTradeMappingContext &context,
    trading::events::Trade &out) noexcept {
  if (!map_trade_to_trade(message.payload, context, out)) {
    return false;
  }

  const auto exchange_seq = out.header.exchange_seq;
  const auto source_ts_ns = out.header.exchange_ts_ns;
  out.header = md::mappers::make_event_header(
      message.envelope, trading::core::EventKind::Trade);
  out.header.source_id = context.source_id;
  out.header.venue_id = context.venue_id;
  out.header.instrument_id = context.instrument_id;
  out.header.exchange_seq = exchange_seq;
  out.header.exchange_ts_ns = source_ts_ns;
  return true;
}

} // 命名空间 md::providers::binance

#pragma once

#include "marketdata/adapters/crypto/binance/detail/json_value_reader.hpp"
#include "marketdata/adapters/crypto/binance/mapping_context.hpp"
#include "marketdata/adapters/event_header.hpp"
#include "marketdata/codecs/fixed_point.hpp"
#include "trading/events/market.hpp"

#include <string_view>

namespace md::adapters::binance {

[[nodiscard]] inline bool map_book_ticker_to_quote(
    std::string_view payload, const BinanceBookTickerMappingContext &context,
    trading::events::Quote &out) noexcept {
  std::string_view symbol{};
  std::string_view bid_price_text{};
  std::string_view bid_quantity_text{};
  std::string_view ask_price_text{};
  std::string_view ask_quantity_text{};
  std::uint64_t update_id = 0;

  if (!detail::read_json_string(payload, "s", symbol) ||
      !detail::read_json_u64(payload, "u", update_id) ||
      !detail::read_json_string(payload, "b", bid_price_text) ||
      !detail::read_json_string(payload, "B", bid_quantity_text) ||
      !detail::read_json_string(payload, "a", ask_price_text) ||
      !detail::read_json_string(payload, "A", ask_quantity_text)) {
    return false;
  }

  if (!context.provider_symbol.empty() &&
      !detail::ascii_equal_ignore_case(symbol, context.provider_symbol)) {
    return false;
  }

  std::int64_t bid_price = 0;
  std::int64_t bid_quantity = 0;
  std::int64_t ask_price = 0;
  std::int64_t ask_quantity = 0;
  if (!md::codecs::parse_decimal_to_fixed(bid_price_text, context.price_scale,
                                        bid_price) ||
      !md::codecs::parse_decimal_to_fixed(bid_quantity_text,
                                        context.quantity_scale, bid_quantity) ||
      !md::codecs::parse_decimal_to_fixed(ask_price_text, context.price_scale,
                                        ask_price) ||
      !md::codecs::parse_decimal_to_fixed(ask_quantity_text,
                                        context.quantity_scale, ask_quantity)) {
    return false;
  }

  trading::events::Quote quote{};
  quote.header.source_id = context.source_id;
  quote.header.venue_id = context.venue_id;
  quote.header.instrument_id = context.instrument_id;
  quote.header.exchange_seq = update_id;
  quote.bid_price = bid_price;
  quote.bid_qty = bid_quantity;
  quote.ask_price = ask_price;
  quote.ask_qty = ask_quantity;
  out = quote;
  return true;
}

[[nodiscard]] inline bool map_book_ticker_to_quote(
    const md::feed::FeedMessageView &message,
    const BinanceBookTickerMappingContext &context,
    trading::events::Quote &out) noexcept {
  if (!map_book_ticker_to_quote(message.payload, context, out)) {
    return false;
  }

  const auto exchange_seq = out.header.exchange_seq;
  out.header = md::adapters::make_event_header(
      message.envelope, trading::core::EventKind::Quote);
  out.header.source_id = context.source_id;
  out.header.venue_id = context.venue_id;
  out.header.instrument_id = context.instrument_id;
  out.header.exchange_seq = exchange_seq;
  return true;
}

} // 命名空间 md::adapters::binance

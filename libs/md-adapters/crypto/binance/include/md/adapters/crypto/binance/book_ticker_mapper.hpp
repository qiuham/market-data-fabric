#pragma once

#include "md/adapters/crypto/binance/feeds.hpp"
#include "md/adapters/crypto/binance/detail/json_value_reader.hpp"
#include "md/core/events.hpp"
#include "md/core/fixed_point.hpp"

#include <cstdint>
#include <string_view>

namespace md::adapters::crypto::binance {

struct BinanceBookTickerMappingContext {
  std::uint32_t source_id{kBinanceSourceId};
  std::uint32_t venue_id{static_cast<std::uint32_t>(md::core::VenueId::Binance)};
  std::uint32_t instrument_id{};
  std::int32_t price_scale{8};
  std::int32_t quantity_scale{8};
  std::string_view provider_symbol{};
};

[[nodiscard]] inline bool map_book_ticker_to_quote(
    std::string_view payload, const BinanceBookTickerMappingContext &context,
    md::core::Quote &out) noexcept {
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
  if (!md::core::parse_decimal_to_fixed(bid_price_text, context.price_scale,
                                        bid_price) ||
      !md::core::parse_decimal_to_fixed(bid_quantity_text,
                                        context.quantity_scale, bid_quantity) ||
      !md::core::parse_decimal_to_fixed(ask_price_text, context.price_scale,
                                        ask_price) ||
      !md::core::parse_decimal_to_fixed(ask_quantity_text,
                                        context.quantity_scale, ask_quantity)) {
    return false;
  }

  md::core::Quote quote{};
  quote.header.source_id = context.source_id;
  quote.header.venue_id = context.venue_id;
  quote.header.instrument_id = context.instrument_id;
  quote.header.exchange_seq = update_id;
  quote.bid_price = bid_price;
  quote.bid_quantity = bid_quantity;
  quote.ask_price = ask_price;
  quote.ask_quantity = ask_quantity;
  out = quote;
  return true;
}

[[nodiscard]] inline bool map_book_ticker_to_quote(
    const md::core::FeedMessageView &message,
    const BinanceBookTickerMappingContext &context,
    md::core::Quote &out) noexcept {
  if (!map_book_ticker_to_quote(message.payload, context, out)) {
    return false;
  }

  const auto exchange_seq = out.header.exchange_seq;
  out.header = message.envelope;
  out.header.payload_kind = md::core::PayloadKind::Quote;
  out.header.payload_encoding = md::core::PayloadEncoding::MdStruct;
  out.header.source_id = context.source_id;
  out.header.venue_id = context.venue_id;
  out.header.instrument_id = context.instrument_id;
  out.header.exchange_seq = exchange_seq;
  return true;
}

} // 命名空间 md::adapters::crypto::binance

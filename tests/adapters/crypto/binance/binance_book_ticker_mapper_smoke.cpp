#include "marketdata/adapters/binance/book_ticker_mapper.hpp"

#include <cassert>
#include <cstdint>
#include <string_view>

int main() {
  namespace binance = md::adapters::binance;

  std::int64_t fixed = 0;
  assert(md::codecs::parse_decimal_to_fixed("25.35190000", 8, fixed));
  assert(fixed == 2535190000LL);
  assert(md::codecs::parse_decimal_to_fixed("25.3519000000", 8, fixed));
  assert(fixed == 2535190000LL);
  assert(!md::codecs::parse_decimal_to_fixed("25.351900001", 8, fixed));

  binance::BinanceBookTickerMappingContext context{};
  context.instrument_id = 1001;
  context.price_scale = 8;
  context.quantity_scale = 8;
  context.provider_symbol = "bnbusdt";

  constexpr std::string_view payload{
      R"({"u":400900217,"s":"BNBUSDT","b":"25.35190000","B":"31.21000000","a":"25.36520000","A":"40.66000000"})"};

  trading::events::Quote quote{};
  assert(binance::map_book_ticker_to_quote(payload, context, quote));
  assert(quote.header.kind == trading::core::EventKind::Quote);
  assert(quote.header.source_id == binance::kBinanceSourceId);
  assert(quote.header.venue_id ==
         static_cast<std::uint32_t>(md::feed::VenueId::Binance));
  assert(quote.header.instrument_id == 1001);
  assert(quote.header.exchange_seq == 400900217);
  assert(quote.bid_price == 2535190000LL);
  assert(quote.bid_qty == 3121000000LL);
  assert(quote.ask_price == 2536520000LL);
  assert(quote.ask_qty == 4066000000LL);

  constexpr std::string_view combined_payload{
      R"({"stream":"bnbusdt@bookTicker","data":{"u":400900218,"s":"BNBUSDT","b":"25.35000000","B":"1.00000000","a":"25.36000000","A":"2.50000000"}})"};
  assert(binance::map_book_ticker_to_quote(combined_payload, context, quote));
  assert(quote.header.exchange_seq == 400900218);
  assert(quote.bid_price == 2535000000LL);
  assert(quote.bid_qty == 100000000LL);
  assert(quote.ask_price == 2536000000LL);
  assert(quote.ask_qty == 250000000LL);

  md::feed::FeedMessageView message{};
  message.envelope.payload_kind = md::feed::PayloadKind::RawProviderMessage;
  message.envelope.payload_encoding = md::feed::PayloadEncoding::ProviderJson;
  message.envelope.connection_id = 7;
  message.envelope.feed_id = 8;
  message.envelope.capture_seq = 9;
  message.envelope.recv_ts_ns = 10;
  message.payload = payload;
  assert(binance::map_book_ticker_to_quote(message, context, quote));
  assert(quote.header.kind == trading::core::EventKind::Quote);
  assert(quote.header.connection_id == 7);
  assert(quote.header.feed_id == 8);
  assert(quote.header.capture_seq == 9);
  assert(quote.header.recv_ts_ns == 10);
  assert(quote.header.exchange_seq == 400900217);

  context.provider_symbol = "BTCUSDT";
  assert(!binance::map_book_ticker_to_quote(payload, context, quote));
  assert(!binance::map_book_ticker_to_quote("{}", context, quote));

  return 0;
}

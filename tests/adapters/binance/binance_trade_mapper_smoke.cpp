#include "md/adapters/crypto/binance/trade_mapper.hpp"

#include <cassert>
#include <cstdint>
#include <string_view>

int main() {
  namespace binance = md::adapters::crypto::binance;

  binance::BinanceTradeMappingContext context{};
  context.instrument_id = 1001;
  context.price_scale = 8;
  context.quantity_scale = 8;
  context.provider_symbol = "bnbusdt";

  constexpr std::string_view sell_payload{
      R"({"e":"trade","E":1672515782136,"s":"BNBUSDT","t":12345,"p":"25.35190000","q":"31.21000000","T":1672515782135,"m":true,"M":true})"};

  md::core::Trade trade{};
  assert(binance::map_trade_to_trade(sell_payload, context, trade));
  assert(trade.header.payload_kind == md::core::PayloadKind::Trade);
  assert(trade.header.payload_encoding == md::core::PayloadEncoding::MdStruct);
  assert(trade.header.source_id == binance::kBinanceSourceId);
  assert(trade.header.venue_id ==
         static_cast<std::uint32_t>(md::core::VenueId::Binance));
  assert(trade.header.instrument_id == 1001);
  assert(trade.header.exchange_seq == 12345);
  assert(trade.header.source_ts_ns == 1672515782135000000ULL);
  assert(trade.trade_id == 12345);
  assert(trade.price == 2535190000LL);
  assert(trade.quantity == 3121000000LL);
  assert(trade.aggressor_side == md::core::AggressorSide::Sell);

  constexpr std::string_view buy_payload{
      R"({"e":"trade","E":1672515782136,"s":"BNBUSDT","t":12346,"p":"25.35000000","q":"1.00000000","T":1672515782136,"m":false,"M":true})"};
  assert(binance::map_trade_to_trade(buy_payload, context, trade));
  assert(trade.header.exchange_seq == 12346);
  assert(trade.trade_id == 12346);
  assert(trade.price == 2535000000LL);
  assert(trade.quantity == 100000000LL);
  assert(trade.aggressor_side == md::core::AggressorSide::Buy);

  constexpr std::string_view combined_payload{
      R"({"stream":"bnbusdt@trade","data":{"e":"trade","E":1672515782136,"s":"BNBUSDT","t":12347,"p":"25.36000000","q":"2.50000000","T":1672515782137,"m":false,"M":true}})"};
  assert(binance::map_trade_to_trade(combined_payload, context, trade));
  assert(trade.header.exchange_seq == 12347);
  assert(trade.header.source_ts_ns == 1672515782137000000ULL);
  assert(trade.price == 2536000000LL);
  assert(trade.quantity == 250000000LL);

  md::core::FeedMessageView message{};
  message.envelope.payload_kind = md::core::PayloadKind::RawProviderMessage;
  message.envelope.payload_encoding = md::core::PayloadEncoding::ProviderJson;
  message.envelope.connection_id = 7;
  message.envelope.feed_id = 8;
  message.envelope.capture_seq = 9;
  message.envelope.recv_ts_ns = 10;
  message.payload = sell_payload;
  assert(binance::map_trade_to_trade(message, context, trade));
  assert(trade.header.payload_kind == md::core::PayloadKind::Trade);
  assert(trade.header.payload_encoding == md::core::PayloadEncoding::MdStruct);
  assert(trade.header.connection_id == 7);
  assert(trade.header.feed_id == 8);
  assert(trade.header.capture_seq == 9);
  assert(trade.header.recv_ts_ns == 10);
  assert(trade.header.exchange_seq == 12345);
  assert(trade.header.source_ts_ns == 1672515782135000000ULL);

  context.provider_symbol = "BTCUSDT";
  assert(!binance::map_trade_to_trade(sell_payload, context, trade));
  assert(!binance::map_trade_to_trade("{}", context, trade));
  context.provider_symbol = "BNBUSDT";
  assert(!binance::map_trade_to_trade(
      R"({"s":"BNBUSDT","t":1,"p":"1.0","q":"1.0","T":1,"m":"false"})",
      context, trade));

  return 0;
}

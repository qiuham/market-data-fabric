#include "md/adapters/crypto/binance/feeds.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
  namespace binance = md::adapters::crypto::binance;

  binance::BinanceFeedSpec trade_spec{};
  trade_spec.market = md::core::MarketSegment::Spot;
  trade_spec.symbols = {"BTCUSDT"};
  trade_spec.kind = md::core::FeedKind::Trade;
  const auto trade = binance::resolve_feed(trade_spec);
  assert(trade.provider_symbol == "btcusdt");
  assert(trade.provider_feed_key == "btcusdt@trade");
  assert(trade.spec.symbols.size() == 1);
  assert(trade.spec.symbols[0] == "BTCUSDT");
  assert(trade.spec.venue == md::core::VenueId::Binance);
  assert(trade.feed_id == md::core::stable_id32(
                              binance::make_feed_id_seed(
                                  trade.spec.market, trade.provider_feed_key)));

  binance::BinanceFeedSpec agg_trade_spec = trade_spec;
  agg_trade_spec.kind = md::core::FeedKind::AggregateTrade;
  const auto agg_trade = binance::resolve_feed(agg_trade_spec);
  assert(agg_trade.provider_feed_key == "btcusdt@aggTrade");

  binance::BinanceFeedSpec depth_spec{};
  depth_spec.market = md::core::MarketSegment::Spot;
  depth_spec.symbols = {"ETHUSDT"};
  depth_spec.kind = md::core::FeedKind::BookUpdate;
  depth_spec.speed = md::core::FeedSpeed::Ms100;
  const auto depth = binance::resolve_feed(depth_spec);
  assert(depth.provider_feed_key == "ethusdt@depth@100ms");

  binance::BinanceFeedSpec partial_depth_spec{};
  partial_depth_spec.market = md::core::MarketSegment::Spot;
  partial_depth_spec.symbols = {"BNBUSDT"};
  partial_depth_spec.kind = md::core::FeedKind::BookSnapshot;
  partial_depth_spec.depth = md::core::FeedDepth::Top20;
  partial_depth_spec.speed = md::core::FeedSpeed::Ms100;
  const auto partial_depth = binance::resolve_feed(partial_depth_spec);
  assert(partial_depth.provider_feed_key == "bnbusdt@depth20@100ms");

  binance::BinanceFeedSpec book_top_spec{};
  book_top_spec.market = md::core::MarketSegment::Spot;
  book_top_spec.symbols = {"BTCUSDT"};
  book_top_spec.kind = md::core::FeedKind::TopOfBook;
  const auto single_connection = binance::make_connection_spec(
      42, binance::BinanceEnvironment::MarketDataOnly, book_top_spec);
  assert(single_connection.endpoint ==
         "wss://data-stream.binance.vision:443/ws/btcusdt@bookTicker");
  assert(single_connection.connection_id == 42);
  assert(single_connection.feeds.size() == 1);
  assert(single_connection.feeds[0].provider_feed_key == "btcusdt@bookTicker");

  binance::BinanceFeedSpec multi_symbol_spec = book_top_spec;
  multi_symbol_spec.symbols = {"BTCUSDT", "ETHUSDT"};
  const auto multi_symbol_connection = binance::make_connection_spec(
      43, binance::BinanceEnvironment::MarketDataOnly, multi_symbol_spec);
  assert(multi_symbol_connection.endpoint ==
         "wss://data-stream.binance.vision:443/"
         "stream?streams=btcusdt@bookTicker/ethusdt@bookTicker");
  assert(multi_symbol_connection.feeds.size() == 2);

  binance::BinanceFeedSpec all_symbols_spec = book_top_spec;
  all_symbols_spec.symbols = {};
  const auto all_symbols_connection = binance::make_connection_spec(
      44, binance::BinanceEnvironment::MarketDataOnly, all_symbols_spec);
  assert(all_symbols_connection.endpoint ==
         "wss://data-stream.binance.vision:443/ws/!bookTicker");
  assert(all_symbols_connection.feeds.size() == 1);
  assert(all_symbols_connection.feeds[0].provider_feed_key == "!bookTicker");

  all_symbols_spec.symbols = {"ALL"};
  const auto all_token_connection = binance::make_connection_spec(
      45, binance::BinanceEnvironment::MarketDataOnly, all_symbols_spec);
  assert(all_token_connection.endpoint ==
         "wss://data-stream.binance.vision:443/ws/!bookTicker");

  const std::vector<binance::BinanceResolvedFeed> feeds{trade, depth};
  const auto multi_connection = binance::make_connection_spec(
      7, binance::BinanceEnvironment::Production, feeds);
  assert(multi_connection.endpoint == "wss://stream.binance.com:9443/"
                                      "stream?streams=btcusdt@trade/"
                                      "ethusdt@depth@100ms");

  const auto connection_envelope =
      md::core::make_connection_envelope(multi_connection, 3, 456);
  assert(connection_envelope.connection_id == 7);
  assert(connection_envelope.feed_id == 0);
  assert((connection_envelope.flags &
          md::core::message_flag(md::core::MessageFlag::FeedKnownWithoutParsing)) ==
         0);

  const auto envelope = md::core::make_feed_envelope(
      single_connection, single_connection.feeds[0], 9, 123);
  assert(envelope.payload_kind == md::core::PayloadKind::RawProviderMessage);
  assert(envelope.payload_encoding == md::core::PayloadEncoding::ProviderJson);
  assert(envelope.source_id == binance::kBinanceSourceId);
  assert(envelope.connection_id == 42);
  assert(envelope.feed_id == single_connection.feeds[0].feed_id);
  assert(envelope.capture_seq == 9);
  assert(envelope.recv_ts_ns == 123);
  return 0;
}

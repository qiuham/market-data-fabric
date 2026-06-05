#include "md/adapters/crypto/binance/spot_streams.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
    namespace binance = md::adapters::crypto::binance;

    const auto trade = binance::make_spot_stream_name("BTCUSDT", binance::SpotStreamKind::Trade);
    assert(trade == "btcusdt@trade");

    const auto agg_trade = binance::make_spot_stream_name("BTCUSDT", binance::SpotStreamKind::AggregateTrade);
    assert(agg_trade == "btcusdt@aggTrade");

    const auto depth = binance::make_spot_stream_name("ETHUSDT", binance::SpotStreamKind::DiffDepth,
                                                       binance::SpotDepthLevels::Twenty,
                                                       binance::SpotDepthSpeed::Ms100);
    assert(depth == "ethusdt@depth@100ms");

    const auto partial_depth = binance::make_spot_stream_name("BNBUSDT", binance::SpotStreamKind::PartialDepth,
                                                               binance::SpotDepthLevels::Twenty,
                                                               binance::SpotDepthSpeed::Ms100);
    assert(partial_depth == "bnbusdt@depth20@100ms");

    const auto subscription = binance::make_spot_raw_subscription(
        "BTCUSDT", binance::SpotStreamKind::BookTicker, 42, binance::SpotEnvironment::MarketDataOnly);
    assert(subscription.stream_name == "btcusdt@bookTicker");
    assert(subscription.url == "wss://data-stream.binance.vision:443/ws/btcusdt@bookTicker");
    assert(subscription.connection_id == 42);
    assert(subscription.stream_id == binance::fnv1a32(subscription.stream_name));

    const std::vector<std::string> streams{trade, depth};
    const auto combined = binance::make_spot_combined_ws_url(binance::SpotEnvironment::Production, streams);
    assert(combined == "wss://stream.binance.com:9443/stream?streams=btcusdt@trade/ethusdt@depth@100ms");

    const auto envelope = binance::make_spot_raw_envelope(subscription, 9, 123);
    assert(envelope.source_id == binance::kBinanceSpotSourceId);
    assert(envelope.connection_id == 42);
    assert(envelope.capture_seq == 9);
    assert(envelope.recv_ts_ns == 123);
    return 0;
}

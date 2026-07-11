#include "marketdata/adapters/binance/feeds.hpp"
#include "marketdata/replay/message_log.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

int main() {
  namespace binance = md::adapters::binance;

  const auto unique =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto path =
      std::filesystem::temp_directory_path() /
      ("mdf_message_log_smoke_" + std::to_string(unique) + ".mdfmsg");

  binance::BinanceFeedSpec spec{};
  spec.market = md::feed::MarketSegment::Spot;
  spec.symbols = {"BTCUSDT"};
  spec.kind = md::feed::FeedKind::Trade;
  const auto connection = binance::make_connection_spec(
      1, binance::BinanceEnvironment::MarketDataOnly, spec);
  const auto &feed = connection.feeds.front();
  const std::string payload = R"({"e":"trade","s":"BTCUSDT"})";

  md::replay::MessageLogWriterOptions options{};
  options.source_id = connection.source_id;
  options.writer_id = 7;
  options.max_total_bytes = sizeof(md::replay::MessageLogFileHeader) +
                            sizeof(md::replay::MessageLogFrameHeader) +
                            payload.size() + 1;
  options.limit_policy = md::replay::MessageLogLimitPolicy::StopRecording;

  md::replay::MessageLogWriter writer{};
  assert(writer.open(path, options) ==
         md::replay::MessageLogAppendStatus::Written);

  auto envelope = md::feed::make_feed_envelope(connection, feed, 1, 123456789);
  auto first = writer.append(envelope, payload);
  assert(first.status == md::replay::MessageLogAppendStatus::Written);
  assert(first.frames_written == 1);

  envelope.capture_seq = 2;
  auto second = writer.append(envelope, payload);
  assert(second.status == md::replay::MessageLogAppendStatus::StoppedByLimit);
  assert(writer.limited());
  writer.close();

  md::replay::MessageLogReader reader{};
  assert(reader.open(path) == md::replay::MessageLogReadStatus::Ok);
  assert(reader.file_header().source_id == connection.source_id);
  assert(reader.file_header().writer_id == 7);

  md::replay::MessageLogRecord record{};
  assert(reader.read_next(record) == md::replay::MessageLogReadStatus::Ok);
  const auto stored = record.envelope();
  assert(stored.payload_kind == md::feed::PayloadKind::RawProviderMessage);
  assert(stored.payload_encoding == md::feed::PayloadEncoding::ProviderJson);
  assert(stored.source_id == connection.source_id);
  assert(stored.connection_id == 1);
  assert(stored.feed_id == feed.feed_id);
  assert(stored.capture_seq == 1);
  assert(stored.payload_size == payload.size());
  assert(record.payload_as_string() == payload);
  assert(reader.read_next(record) == md::replay::MessageLogReadStatus::Eof);

  reader.close();
  std::filesystem::remove(path);
  return 0;
}

#include "md/adapters/crypto/binance/spot_streams.hpp"
#include "md/replay/message_log.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

int main() {
  namespace binance = md::adapters::crypto::binance;

  const auto unique =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto path =
      std::filesystem::temp_directory_path() /
      ("mdf_message_log_smoke_" + std::to_string(unique) + ".mdfmsg");

  const auto subscription = binance::make_spot_subscription(
      "BTCUSDT", binance::SpotStreamKind::Trade, 1,
      binance::SpotEnvironment::MarketDataOnly);
  const std::string payload = R"({"e":"trade","s":"BTCUSDT"})";

  md::replay::MessageLogWriterOptions options{};
  options.source_id = subscription.source_id;
  options.writer_id = 7;
  options.max_total_bytes = sizeof(md::replay::MessageLogFileHeader) +
                            sizeof(md::replay::MessageLogFrameHeader) +
                            payload.size() + 1;
  options.limit_policy = md::replay::MessageLogLimitPolicy::StopRecording;

  md::replay::MessageLogWriter writer{};
  assert(writer.open(path, options) ==
         md::replay::MessageLogAppendStatus::Written);

  auto envelope = binance::make_spot_envelope(subscription, 1, 123456789);
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
  assert(reader.file_header().source_id == subscription.source_id);
  assert(reader.file_header().writer_id == 7);

  md::replay::MessageLogRecord record{};
  assert(reader.read_next(record) == md::replay::MessageLogReadStatus::Ok);
  const auto stored = record.envelope();
  assert(stored.payload_kind == md::core::PayloadKind::ProviderMessage);
  assert(stored.payload_encoding == md::core::PayloadEncoding::ProviderJson);
  assert(stored.source_id == subscription.source_id);
  assert(stored.connection_id == 1);
  assert(stored.stream_id == subscription.stream_id);
  assert(stored.capture_seq == 1);
  assert(stored.payload_size == payload.size());
  assert(record.payload_as_string() == payload);
  assert(reader.read_next(record) == md::replay::MessageLogReadStatus::Eof);

  reader.close();
  std::filesystem::remove(path);
  return 0;
}

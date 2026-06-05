#include "md/adapters/crypto/binance/spot_streams.hpp"
#include "md/core/raw.hpp"
#include "md/replay/raw_reader.hpp"
#include "md/replay/raw_writer.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

int main() {
    namespace binance = md::adapters::crypto::binance;

    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
                      ("mdf_raw_file_smoke_" + std::to_string(unique) + ".mdfraw");

    const auto subscription = binance::make_spot_raw_subscription(
        "BTCUSDT", binance::SpotStreamKind::Trade, 1, binance::SpotEnvironment::MarketDataOnly);
    const std::string payload = R"({"e":"trade","s":"BTCUSDT"})";

    md::replay::RawWriterOptions options{};
    options.source_id = subscription.source_id;
    options.writer_id = 7;
    options.max_total_bytes = sizeof(md::replay::RawFileHeader) + sizeof(md::replay::RawFrameHeader) +
                              payload.size() + 1;
    options.limit_policy = md::replay::RawStorageLimitPolicy::StopRecording;

    md::replay::RawWriter writer{};
    assert(writer.open(path, options) == md::replay::RawAppendStatus::Written);

    auto envelope = binance::make_spot_raw_envelope(subscription, 1, 123456789);
    auto first = writer.append(envelope, payload);
    assert(first.status == md::replay::RawAppendStatus::Written);
    assert(first.frames_written == 1);

    envelope.capture_seq = 2;
    auto second = writer.append(envelope, payload);
    assert(second.status == md::replay::RawAppendStatus::StoppedByLimit);
    assert(writer.limited());
    writer.close();

    md::replay::RawReader reader{};
    assert(reader.open(path) == md::replay::RawReadStatus::Ok);
    assert(reader.file_header().source_id == subscription.source_id);
    assert(reader.file_header().writer_id == 7);

    md::replay::RawRecord record{};
    assert(reader.read_next(record) == md::replay::RawReadStatus::Ok);
    assert(record.header.source_id == subscription.source_id);
    assert(record.header.connection_id == 1);
    assert(record.header.stream_id == subscription.stream_id);
    assert(record.header.capture_seq == 1);
    assert(record.payload_as_string() == payload);
    assert(reader.read_next(record) == md::replay::RawReadStatus::Eof);

    reader.close();
    std::filesystem::remove(path);
    return 0;
}

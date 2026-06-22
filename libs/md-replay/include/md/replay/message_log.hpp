#pragma once

#include "md/core/message.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace md::replay {

inline constexpr std::array<char, 8> kMessageLogMagic{'M', 'D', 'F', 'M',
                                                      'S', 'G', '1', '\0'};
inline constexpr std::array<char, 4> kMessageFrameMagic{'M', 'S', 'G', 'F'};
inline constexpr std::uint16_t kMessageLogFormatVersion{1};
inline constexpr std::uint32_t kMessageLogEndianCheck{0x01020304u};
inline constexpr std::uint64_t kDefaultMessageLogMaxBytes =
    100ull * 1024ull * 1024ull;
inline constexpr std::uint32_t kDefaultMessageLogMaxPayloadBytes =
    100u * 1024u * 1024u;

#pragma pack(push, 1)
struct MessageLogFileHeader {
  char magic[8]{'M', 'D', 'F', 'M', 'S', 'G', '1', '\0'};
  std::uint16_t version{kMessageLogFormatVersion};
  std::uint16_t header_size{sizeof(MessageLogFileHeader)};
  std::uint32_t flags{};
  std::uint32_t endian_check{kMessageLogEndianCheck};
  std::uint64_t created_ts_ns{};
  std::uint32_t source_id{};
  std::uint32_t writer_id{};
  std::uint64_t max_total_bytes{};
  std::uint8_t reserved[20]{};
};

struct MessageLogFrameHeader {
  char magic[4]{'M', 'S', 'G', 'F'};
  std::uint16_t version{kMessageLogFormatVersion};
  std::uint16_t header_size{sizeof(MessageLogFrameHeader)};
  std::uint16_t payload_kind{};
  std::uint8_t payload_encoding{};
  std::uint8_t protocol{};
  std::uint8_t compression{};
  std::uint8_t reserved0{};
  std::uint32_t payload_size{};
  std::uint32_t source_id{};
  std::uint32_t venue_id{};
  std::uint32_t connection_id{};
  std::uint32_t feed_id{};
  std::uint32_t instrument_id{};
  std::uint64_t capture_seq{};
  std::uint64_t event_seq{};
  std::uint64_t exchange_seq{};
  std::uint64_t source_ts_ns{};
  std::uint64_t recv_ts_ns{};
  std::uint64_t publish_ts_ns{};
  std::uint32_t flags{};
  std::uint8_t reserved[38]{};
};
#pragma pack(pop)

static_assert(sizeof(MessageLogFileHeader) == 64);
static_assert(sizeof(MessageLogFrameHeader) == 128);
static_assert(std::is_standard_layout_v<MessageLogFileHeader>);
static_assert(std::is_standard_layout_v<MessageLogFrameHeader>);

inline bool has_message_log_magic(const MessageLogFileHeader &header) noexcept {
  return std::memcmp(header.magic, kMessageLogMagic.data(),
                     kMessageLogMagic.size()) == 0;
}

inline bool
has_message_frame_magic(const MessageLogFrameHeader &header) noexcept {
  return std::memcmp(header.magic, kMessageFrameMagic.data(),
                     kMessageFrameMagic.size()) == 0;
}

inline MessageLogFileHeader
make_message_log_file_header(std::uint64_t created_ts_ns,
                             std::uint32_t source_id, std::uint32_t writer_id,
                             std::uint64_t max_total_bytes) noexcept {
  MessageLogFileHeader header{};
  header.created_ts_ns = created_ts_ns;
  header.source_id = source_id;
  header.writer_id = writer_id;
  header.max_total_bytes = max_total_bytes;
  return header;
}

inline MessageLogFrameHeader
make_message_log_frame_header(const md::core::MessageEnvelope &envelope,
                              std::uint64_t payload_size) noexcept {
  MessageLogFrameHeader header{};
  header.payload_kind = static_cast<std::uint16_t>(envelope.payload_kind);
  header.payload_encoding =
      static_cast<std::uint8_t>(envelope.payload_encoding);
  header.protocol = static_cast<std::uint8_t>(envelope.protocol);
  header.compression = static_cast<std::uint8_t>(envelope.compression);
  header.payload_size = payload_size > std::numeric_limits<std::uint32_t>::max()
                            ? std::numeric_limits<std::uint32_t>::max()
                            : static_cast<std::uint32_t>(payload_size);
  header.source_id = envelope.source_id;
  header.venue_id = envelope.venue_id;
  header.connection_id = envelope.connection_id;
  header.feed_id = envelope.feed_id;
  header.instrument_id = envelope.instrument_id;
  header.capture_seq = envelope.capture_seq;
  header.event_seq = envelope.event_seq;
  header.exchange_seq = envelope.exchange_seq;
  header.source_ts_ns = envelope.source_ts_ns;
  header.recv_ts_ns = envelope.recv_ts_ns;
  header.publish_ts_ns = envelope.publish_ts_ns;
  header.flags = envelope.flags;
  return header;
}

inline md::core::MessageEnvelope
to_message_envelope(const MessageLogFrameHeader &header) noexcept {
  md::core::MessageEnvelope envelope{};
  envelope.payload_kind =
      static_cast<md::core::PayloadKind>(header.payload_kind);
  envelope.payload_encoding =
      static_cast<md::core::PayloadEncoding>(header.payload_encoding);
  envelope.protocol = static_cast<md::core::TransportProtocol>(header.protocol);
  envelope.compression = static_cast<md::core::Compression>(header.compression);
  envelope.payload_size = header.payload_size;
  envelope.source_id = header.source_id;
  envelope.venue_id = header.venue_id;
  envelope.connection_id = header.connection_id;
  envelope.feed_id = header.feed_id;
  envelope.instrument_id = header.instrument_id;
  envelope.capture_seq = header.capture_seq;
  envelope.event_seq = header.event_seq;
  envelope.exchange_seq = header.exchange_seq;
  envelope.source_ts_ns = header.source_ts_ns;
  envelope.recv_ts_ns = header.recv_ts_ns;
  envelope.publish_ts_ns = header.publish_ts_ns;
  envelope.flags = header.flags;
  return envelope;
}

enum class MessageLogLimitPolicy : std::uint8_t {
  StopRecording = 0,
  DropNew = 1,
  RotateDeleteOldest = 2,
};

enum class MessageLogAppendStatus : std::uint8_t {
  Written = 0,
  DroppedByLimit = 1,
  StoppedByLimit = 2,
  WriterClosed = 3,
  PayloadTooLarge = 4,
  IoError = 5,
  RotateNotImplemented = 6,
};

struct MessageLogWriterOptions {
  std::uint64_t max_total_bytes{kDefaultMessageLogMaxBytes};
  MessageLogLimitPolicy limit_policy{MessageLogLimitPolicy::StopRecording};
  std::uint32_t source_id{};
  std::uint32_t writer_id{};
};

struct MessageLogAppendResult {
  MessageLogAppendStatus status{MessageLogAppendStatus::WriterClosed};
  std::uint64_t bytes_written{};
  std::uint64_t frames_written{};
};

class MessageLogWriter {
public:
  MessageLogWriter() = default;

  MessageLogWriter(const std::filesystem::path &path,
                   const MessageLogWriterOptions &options) {
    (void)open(path, options);
  }

  ~MessageLogWriter() { close(); }

  MessageLogAppendStatus open(const std::filesystem::path &path,
                              const MessageLogWriterOptions &options) {
    close();
    options_ = options;
    limited_ = false;
    bytes_written_ = 0;
    frames_written_ = 0;

    if (const auto parent = path.parent_path(); !parent.empty()) {
      std::filesystem::create_directories(parent);
    }

    out_.open(path, std::ios::binary | std::ios::trunc);
    if (!out_) {
      return MessageLogAppendStatus::IoError;
    }

    const auto header = make_message_log_file_header(
        now_epoch_ns(), options.source_id, options.writer_id,
        options.max_total_bytes);
    out_.write(reinterpret_cast<const char *>(&header), sizeof(header));
    if (!out_) {
      close();
      return MessageLogAppendStatus::IoError;
    }

    bytes_written_ = sizeof(header);
    return MessageLogAppendStatus::Written;
  }

  MessageLogAppendResult append(const md::core::MessageEnvelope &envelope,
                                std::span<const std::byte> payload) {
    if (!out_.is_open()) {
      return {MessageLogAppendStatus::WriterClosed, bytes_written_,
              frames_written_};
    }
    if (limited_) {
      return {MessageLogAppendStatus::StoppedByLimit, bytes_written_,
              frames_written_};
    }
    if (payload.size() >
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
      return {MessageLogAppendStatus::PayloadTooLarge, bytes_written_,
              frames_written_};
    }

    const std::uint64_t frame_bytes =
        sizeof(MessageLogFrameHeader) + payload.size();
    if (would_exceed_limit(frame_bytes)) {
      return handle_limit();
    }

    auto header = make_message_log_frame_header(envelope, payload.size());
    out_.write(reinterpret_cast<const char *>(&header), sizeof(header));
    if (!payload.empty()) {
      out_.write(reinterpret_cast<const char *>(payload.data()),
                 static_cast<std::streamsize>(payload.size()));
    }
    if (!out_) {
      return {MessageLogAppendStatus::IoError, bytes_written_, frames_written_};
    }

    bytes_written_ += frame_bytes;
    ++frames_written_;
    return {MessageLogAppendStatus::Written, bytes_written_, frames_written_};
  }

  MessageLogAppendResult append(const md::core::MessageEnvelope &envelope,
                                std::string_view payload) {
    const auto *data = reinterpret_cast<const std::byte *>(payload.data());
    return append(envelope, std::span<const std::byte>(data, payload.size()));
  }

  void flush() {
    if (out_.is_open()) {
      out_.flush();
    }
  }

  void close() {
    if (out_.is_open()) {
      out_.flush();
      out_.close();
    }
  }

  [[nodiscard]] bool is_open() const noexcept { return out_.is_open(); }

  [[nodiscard]] bool limited() const noexcept { return limited_; }

  [[nodiscard]] std::uint64_t bytes_written() const noexcept {
    return bytes_written_;
  }

  [[nodiscard]] std::uint64_t frames_written() const noexcept {
    return frames_written_;
  }

private:
  static std::uint64_t now_epoch_ns() noexcept {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
  }

  [[nodiscard]] bool
  would_exceed_limit(std::uint64_t next_frame_bytes) const noexcept {
    if (options_.max_total_bytes == 0) {
      return false;
    }
    if (bytes_written_ > options_.max_total_bytes) {
      return true;
    }
    return next_frame_bytes > options_.max_total_bytes - bytes_written_;
  }

  MessageLogAppendResult handle_limit() {
    switch (options_.limit_policy) {
    case MessageLogLimitPolicy::StopRecording:
      limited_ = true;
      return {MessageLogAppendStatus::StoppedByLimit, bytes_written_,
              frames_written_};
    case MessageLogLimitPolicy::DropNew:
      return {MessageLogAppendStatus::DroppedByLimit, bytes_written_,
              frames_written_};
    case MessageLogLimitPolicy::RotateDeleteOldest:
      limited_ = true;
      return {MessageLogAppendStatus::RotateNotImplemented, bytes_written_,
              frames_written_};
    }
    limited_ = true;
    return {MessageLogAppendStatus::StoppedByLimit, bytes_written_,
            frames_written_};
  }

  MessageLogWriterOptions options_{};
  std::ofstream out_{};
  std::uint64_t bytes_written_{};
  std::uint64_t frames_written_{};
  bool limited_{};
};

enum class MessageLogReadStatus : std::uint8_t {
  Ok = 0,
  Eof = 1,
  InvalidHeader = 2,
  PayloadTooLarge = 3,
  IoError = 4,
};

struct MessageLogReaderOptions {
  std::uint32_t max_payload_bytes{kDefaultMessageLogMaxPayloadBytes};
};

struct MessageLogRecord {
  MessageLogFrameHeader header{};
  std::vector<std::byte> payload{};

  [[nodiscard]] md::core::MessageEnvelope envelope() const noexcept {
    return to_message_envelope(header);
  }

  [[nodiscard]] std::string payload_as_string() const {
    if (payload.empty()) {
      return {};
    }
    return {reinterpret_cast<const char *>(payload.data()), payload.size()};
  }
};

class MessageLogReader {
public:
  MessageLogReader() = default;

  explicit MessageLogReader(const std::filesystem::path &path,
                            MessageLogReaderOptions options = {}) {
    (void)open(path, options);
  }

  MessageLogReadStatus open(const std::filesystem::path &path,
                            MessageLogReaderOptions options = {}) {
    close();
    options_ = options;
    in_.open(path, std::ios::binary);
    if (!in_) {
      return MessageLogReadStatus::IoError;
    }

    in_.read(reinterpret_cast<char *>(&file_header_), sizeof(file_header_));
    if (in_.gcount() != static_cast<std::streamsize>(sizeof(file_header_)) ||
        !has_message_log_magic(file_header_) ||
        file_header_.version != kMessageLogFormatVersion ||
        file_header_.header_size != sizeof(MessageLogFileHeader) ||
        file_header_.endian_check != kMessageLogEndianCheck) {
      close();
      return MessageLogReadStatus::InvalidHeader;
    }

    return MessageLogReadStatus::Ok;
  }

  MessageLogReadStatus read_next(MessageLogRecord &record) {
    if (!in_.is_open()) {
      return MessageLogReadStatus::IoError;
    }

    MessageLogFrameHeader header{};
    in_.read(reinterpret_cast<char *>(&header), sizeof(header));
    if (in_.gcount() == 0 && in_.eof()) {
      return MessageLogReadStatus::Eof;
    }
    if (in_.gcount() != static_cast<std::streamsize>(sizeof(header)) ||
        !has_message_frame_magic(header) ||
        header.version != kMessageLogFormatVersion ||
        header.header_size != sizeof(MessageLogFrameHeader)) {
      return MessageLogReadStatus::InvalidHeader;
    }
    if (header.payload_size > options_.max_payload_bytes) {
      return MessageLogReadStatus::PayloadTooLarge;
    }

    record.header = header;
    record.payload.assign(header.payload_size, std::byte{});
    if (header.payload_size > 0) {
      in_.read(reinterpret_cast<char *>(record.payload.data()),
               header.payload_size);
      if (in_.gcount() != static_cast<std::streamsize>(header.payload_size)) {
        return MessageLogReadStatus::InvalidHeader;
      }
    }
    return MessageLogReadStatus::Ok;
  }

  void close() {
    if (in_.is_open()) {
      in_.close();
    }
  }

  [[nodiscard]] const MessageLogFileHeader &file_header() const noexcept {
    return file_header_;
  }

private:
  std::ifstream in_{};
  MessageLogFileHeader file_header_{};
  MessageLogReaderOptions options_{};
};

} // namespace md::replay

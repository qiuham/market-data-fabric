#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace md::core {

enum class PayloadKind : std::uint16_t {
  Unknown = 0,
  RawProviderMessage = 1,
  Trade = 2,
  Quote = 3,
  BookUpdate = 4,
  BookDelta = BookUpdate,
  BookSnapshot = 5,
  Status = 6,
  Heartbeat = 7,
  OrderEvent = 8,
  Execution = 9,
  Control = 10,
  Error = 11,
};

enum class PayloadEncoding : std::uint8_t {
  Unknown = 0,
  ProviderJson = 1,
  ProviderBinary = 2,
  MdStruct = 3,
  Protobuf = 4,
  FlatBuffers = 5,
  Sbe = 6,
  Text = 7,
};

enum class TransportProtocol : std::uint8_t {
  Unknown = 0,
  WebSocket = 1,
  Tcp = 2,
  Udp = 3,
  Multicast = 4,
  Sdk = 5,
  File = 6,
  Http = 7,
};

enum class Compression : std::uint8_t {
  None = 0,
  Gzip = 1,
  Zstd = 2,
};

enum class MessageFlag : std::uint32_t {
  None = 0,
  ProviderTimestampUnknown = 1u << 0,
  FeedKnownWithoutParsing = 1u << 1,
  PayloadMayContainMultipleEvents = 1u << 2,
  BinaryPayload = 1u << 3,
};

constexpr std::uint32_t message_flag(MessageFlag flag) noexcept {
  return static_cast<std::uint32_t>(flag);
}

constexpr std::uint32_t operator|(MessageFlag lhs, MessageFlag rhs) noexcept {
  return message_flag(lhs) | message_flag(rhs);
}

struct MessageEnvelope {
  std::uint16_t schema_version{1};
  PayloadKind payload_kind{PayloadKind::Unknown};
  PayloadEncoding payload_encoding{PayloadEncoding::Unknown};
  TransportProtocol protocol{TransportProtocol::Unknown};
  Compression compression{Compression::None};
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
  std::uint32_t payload_size{};
  std::uint32_t flags{};
};

inline constexpr std::size_t kDefaultProviderPayloadBytes = 64u * 1024u;

template <std::size_t MaxBytes = kDefaultProviderPayloadBytes>
struct BytePayload {
  std::uint32_t size{};
  std::array<std::byte, MaxBytes> bytes{};

  [[nodiscard]] static constexpr std::size_t max_bytes() noexcept {
    return MaxBytes;
  }

  [[nodiscard]] bool assign(std::span<const std::byte> data) noexcept {
    if (data.size() > MaxBytes) {
      return false;
    }

    std::copy(data.begin(), data.end(), bytes.begin());
    size = static_cast<std::uint32_t>(data.size());
    return true;
  }

  [[nodiscard]] bool assign(std::string_view text) noexcept {
    const auto *data = reinterpret_cast<const std::byte *>(text.data());
    return assign(std::span<const std::byte>(data, text.size()));
  }

  [[nodiscard]] std::span<const std::byte> view() const noexcept {
    return {bytes.data(), size};
  }

  [[nodiscard]] std::span<std::byte> mutable_view() noexcept {
    return {bytes.data(), size};
  }

  [[nodiscard]] std::string_view text_view() const noexcept {
    return {reinterpret_cast<const char *>(bytes.data()), size};
  }

  void clear() noexcept { size = 0; }
};

template <typename Payload> struct MessageFrame {
  MessageEnvelope envelope{};
  Payload payload{};
};

template <std::size_t MaxBytes = kDefaultProviderPayloadBytes>
using RawProviderMessageFrame = MessageFrame<BytePayload<MaxBytes>>;

template <std::size_t MaxBytes>
[[nodiscard]] bool assign_payload(RawProviderMessageFrame<MaxBytes> &frame,
                                  std::span<const std::byte> data) noexcept {
  if (!frame.payload.assign(data)) {
    return false;
  }
  frame.envelope.payload_size = frame.payload.size;
  return true;
}

template <std::size_t MaxBytes>
[[nodiscard]] bool assign_payload(RawProviderMessageFrame<MaxBytes> &frame,
                                  std::string_view text) noexcept {
  if (!frame.payload.assign(text)) {
    return false;
  }
  frame.envelope.payload_size = frame.payload.size;
  return true;
}

} // 命名空间 md::core

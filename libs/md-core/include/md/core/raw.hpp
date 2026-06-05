#pragma once

#include <cstdint>

namespace md::core {

enum class RawProtocol : std::uint8_t {
    Unknown = 0,
    WebSocket = 1,
    Tcp = 2,
    Udp = 3,
    Multicast = 4,
    Sdk = 5,
    File = 6,
    Http = 7,
};

enum class RawPayloadCodec : std::uint8_t {
    Unknown = 0,
    JsonText = 1,
    Binary = 2,
    Sbe = 3,
    Text = 4,
};

enum class RawCompression : std::uint8_t {
    None = 0,
    Gzip = 1,
    Zstd = 2,
};

enum class RawMessageKind : std::uint8_t {
    Unknown = 0,
    Data = 1,
    Snapshot = 2,
    Delta = 3,
    Trade = 4,
    Quote = 5,
    Heartbeat = 6,
    SubscribeAck = 7,
    Error = 8,
    Control = 9,
};

enum class RawFrameFlag : std::uint32_t {
    None = 0,
    ProviderTimestampUnknown = 1u << 0,
    StreamKnownWithoutParsing = 1u << 1,
    PayloadMayContainMultipleEvents = 1u << 2,
    BinaryPayload = 1u << 3,
};

constexpr std::uint32_t raw_frame_flag(RawFrameFlag flag) noexcept {
    return static_cast<std::uint32_t>(flag);
}

constexpr std::uint32_t operator|(RawFrameFlag lhs, RawFrameFlag rhs) noexcept {
    return raw_frame_flag(lhs) | raw_frame_flag(rhs);
}

struct RawEnvelope {
    std::uint16_t schema_version{1};
    std::uint32_t source_id{};
    std::uint32_t connection_id{};
    std::uint32_t stream_id{};
    std::uint64_t capture_seq{};
    std::uint64_t recv_ts_ns{};
    RawProtocol protocol{RawProtocol::Unknown};
    RawPayloadCodec payload_codec{RawPayloadCodec::Unknown};
    RawCompression compression{RawCompression::None};
    RawMessageKind message_kind{RawMessageKind::Data};
    std::uint32_t flags{};
};

} // namespace md::core

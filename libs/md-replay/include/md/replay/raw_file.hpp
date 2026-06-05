#pragma once

#include "md/core/raw.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace md::replay {

inline constexpr std::array<char, 8> kRawFileMagic{'M', 'D', 'F', 'R', 'A', 'W', '1', '\0'};
inline constexpr std::array<char, 4> kRawFrameMagic{'R', 'A', 'W', 'F'};
inline constexpr std::uint16_t kRawFileFormatVersion{1};
inline constexpr std::uint32_t kRawEndianCheck{0x01020304u};

#pragma pack(push, 1)
struct RawFileHeader {
    char magic[8]{'M', 'D', 'F', 'R', 'A', 'W', '1', '\0'};
    std::uint16_t version{kRawFileFormatVersion};
    std::uint16_t header_size{sizeof(RawFileHeader)};
    std::uint32_t flags{};
    std::uint32_t endian_check{kRawEndianCheck};
    std::uint64_t created_ts_ns{};
    std::uint32_t source_id{};
    std::uint32_t writer_id{};
    std::uint64_t max_total_bytes{};
    std::uint8_t reserved[20]{};
};

struct RawFrameHeader {
    char magic[4]{'R', 'A', 'W', 'F'};
    std::uint16_t version{kRawFileFormatVersion};
    std::uint16_t header_size{sizeof(RawFrameHeader)};
    std::uint32_t payload_size{};
    std::uint32_t source_id{};
    std::uint32_t connection_id{};
    std::uint32_t stream_id{};
    std::uint64_t capture_seq{};
    std::uint64_t recv_ts_ns{};
    std::uint8_t protocol{};
    std::uint8_t payload_codec{};
    std::uint8_t compression{};
    std::uint8_t message_kind{};
    std::uint32_t flags{};
    std::uint8_t reserved[16]{};
};
#pragma pack(pop)

static_assert(sizeof(RawFileHeader) == 64);
static_assert(sizeof(RawFrameHeader) == 64);
static_assert(std::is_standard_layout_v<RawFileHeader>);
static_assert(std::is_standard_layout_v<RawFrameHeader>);

inline bool has_raw_file_magic(const RawFileHeader& header) noexcept {
    return std::memcmp(header.magic, kRawFileMagic.data(), kRawFileMagic.size()) == 0;
}

inline bool has_raw_frame_magic(const RawFrameHeader& header) noexcept {
    return std::memcmp(header.magic, kRawFrameMagic.data(), kRawFrameMagic.size()) == 0;
}

inline RawFileHeader make_raw_file_header(std::uint64_t created_ts_ns,
                                          std::uint32_t source_id,
                                          std::uint32_t writer_id,
                                          std::uint64_t max_total_bytes) noexcept {
    RawFileHeader header{};
    header.created_ts_ns = created_ts_ns;
    header.source_id = source_id;
    header.writer_id = writer_id;
    header.max_total_bytes = max_total_bytes;
    return header;
}

inline RawFrameHeader make_raw_frame_header(const md::core::RawEnvelope& envelope,
                                            std::uint64_t payload_size) noexcept {
    RawFrameHeader header{};
    header.payload_size = payload_size > std::numeric_limits<std::uint32_t>::max()
                              ? std::numeric_limits<std::uint32_t>::max()
                              : static_cast<std::uint32_t>(payload_size);
    header.source_id = envelope.source_id;
    header.connection_id = envelope.connection_id;
    header.stream_id = envelope.stream_id;
    header.capture_seq = envelope.capture_seq;
    header.recv_ts_ns = envelope.recv_ts_ns;
    header.protocol = static_cast<std::uint8_t>(envelope.protocol);
    header.payload_codec = static_cast<std::uint8_t>(envelope.payload_codec);
    header.compression = static_cast<std::uint8_t>(envelope.compression);
    header.message_kind = static_cast<std::uint8_t>(envelope.message_kind);
    header.flags = envelope.flags;
    return header;
}

inline md::core::RawEnvelope to_raw_envelope(const RawFrameHeader& header) noexcept {
    md::core::RawEnvelope envelope{};
    envelope.source_id = header.source_id;
    envelope.connection_id = header.connection_id;
    envelope.stream_id = header.stream_id;
    envelope.capture_seq = header.capture_seq;
    envelope.recv_ts_ns = header.recv_ts_ns;
    envelope.protocol = static_cast<md::core::RawProtocol>(header.protocol);
    envelope.payload_codec = static_cast<md::core::RawPayloadCodec>(header.payload_codec);
    envelope.compression = static_cast<md::core::RawCompression>(header.compression);
    envelope.message_kind = static_cast<md::core::RawMessageKind>(header.message_kind);
    envelope.flags = header.flags;
    return envelope;
}

} // namespace md::replay

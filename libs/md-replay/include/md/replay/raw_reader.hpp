#pragma once

#include "md/core/raw.hpp"
#include "md/replay/raw_file.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace md::replay {

enum class RawReadStatus : std::uint8_t {
    Ok = 0,
    Eof = 1,
    InvalidHeader = 2,
    PayloadTooLarge = 3,
    IoError = 4,
};

inline constexpr std::uint32_t kDefaultRawMaxPayloadBytes = 100u * 1024u * 1024u;

struct RawReaderOptions {
    std::uint32_t max_payload_bytes{kDefaultRawMaxPayloadBytes};
};

struct RawRecord {
    RawFrameHeader header{};
    std::vector<std::byte> payload{};

    [[nodiscard]] md::core::RawEnvelope envelope() const noexcept {
        return to_raw_envelope(header);
    }

    [[nodiscard]] std::string payload_as_string() const {
        if (payload.empty()) {
            return {};
        }
        return {reinterpret_cast<const char*>(payload.data()), payload.size()};
    }
};

class RawReader {
public:
    RawReader() = default;

    explicit RawReader(const std::filesystem::path& path, RawReaderOptions options = {}) {
        (void)open(path, options);
    }

    RawReadStatus open(const std::filesystem::path& path, RawReaderOptions options = {}) {
        close();
        options_ = options;
        in_.open(path, std::ios::binary);
        if (!in_) {
            return RawReadStatus::IoError;
        }

        in_.read(reinterpret_cast<char*>(&file_header_), sizeof(file_header_));
        if (in_.gcount() != static_cast<std::streamsize>(sizeof(file_header_)) ||
            !has_raw_file_magic(file_header_) || file_header_.version != kRawFileFormatVersion ||
            file_header_.header_size != sizeof(RawFileHeader) || file_header_.endian_check != kRawEndianCheck) {
            close();
            return RawReadStatus::InvalidHeader;
        }

        return RawReadStatus::Ok;
    }

    RawReadStatus read_next(RawRecord& record) {
        if (!in_.is_open()) {
            return RawReadStatus::IoError;
        }

        RawFrameHeader header{};
        in_.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (in_.gcount() == 0 && in_.eof()) {
            return RawReadStatus::Eof;
        }
        if (in_.gcount() != static_cast<std::streamsize>(sizeof(header)) || !has_raw_frame_magic(header) ||
            header.version != kRawFileFormatVersion || header.header_size != sizeof(RawFrameHeader)) {
            return RawReadStatus::InvalidHeader;
        }
        if (header.payload_size > options_.max_payload_bytes) {
            return RawReadStatus::PayloadTooLarge;
        }

        record.header = header;
        record.payload.assign(header.payload_size, std::byte{});
        if (header.payload_size > 0) {
            in_.read(reinterpret_cast<char*>(record.payload.data()), header.payload_size);
            if (in_.gcount() != static_cast<std::streamsize>(header.payload_size)) {
                return RawReadStatus::InvalidHeader;
            }
        }
        return RawReadStatus::Ok;
    }

    void close() {
        if (in_.is_open()) {
            in_.close();
        }
    }

    [[nodiscard]] const RawFileHeader& file_header() const noexcept {
        return file_header_;
    }

private:
    std::ifstream in_{};
    RawFileHeader file_header_{};
    RawReaderOptions options_{};
};

} // namespace md::replay

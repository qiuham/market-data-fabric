#pragma once

#include "md/core/raw.hpp"
#include "md/replay/raw_file.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string_view>

namespace md::replay {

inline constexpr std::uint64_t kDefaultRawMaxTotalBytes = 100ull * 1024ull * 1024ull;

enum class RawStorageLimitPolicy : std::uint8_t {
    StopRecording = 0,
    DropNew = 1,
    RotateDeleteOldest = 2,
};

enum class RawAppendStatus : std::uint8_t {
    Written = 0,
    DroppedByLimit = 1,
    StoppedByLimit = 2,
    WriterClosed = 3,
    PayloadTooLarge = 4,
    IoError = 5,
    RotateNotImplemented = 6,
};

struct RawWriterOptions {
    std::uint64_t max_total_bytes{kDefaultRawMaxTotalBytes};
    RawStorageLimitPolicy limit_policy{RawStorageLimitPolicy::StopRecording};
    std::uint32_t source_id{};
    std::uint32_t writer_id{};
};

struct RawAppendResult {
    RawAppendStatus status{RawAppendStatus::WriterClosed};
    std::uint64_t bytes_written{};
    std::uint64_t frames_written{};
};

class RawWriter {
public:
    RawWriter() = default;

    RawWriter(const std::filesystem::path& path, const RawWriterOptions& options) {
        (void)open(path, options);
    }

    ~RawWriter() {
        close();
    }

    RawAppendStatus open(const std::filesystem::path& path, const RawWriterOptions& options) {
        close();
        options_ = options;
        path_ = path;
        limited_ = false;
        bytes_written_ = 0;
        frames_written_ = 0;

        if (const auto parent = path.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }

        out_.open(path, std::ios::binary | std::ios::trunc);
        if (!out_) {
            return RawAppendStatus::IoError;
        }

        const auto header = make_raw_file_header(now_epoch_ns(), options.source_id, options.writer_id,
                                                 options.max_total_bytes);
        out_.write(reinterpret_cast<const char*>(&header), sizeof(header));
        if (!out_) {
            close();
            return RawAppendStatus::IoError;
        }

        bytes_written_ = sizeof(header);
        return RawAppendStatus::Written;
    }

    RawAppendResult append(const md::core::RawEnvelope& envelope, std::span<const std::byte> payload) {
        if (!out_.is_open()) {
            return {RawAppendStatus::WriterClosed, bytes_written_, frames_written_};
        }
        if (limited_) {
            return {RawAppendStatus::StoppedByLimit, bytes_written_, frames_written_};
        }
        if (payload.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            return {RawAppendStatus::PayloadTooLarge, bytes_written_, frames_written_};
        }

        const std::uint64_t frame_bytes = sizeof(RawFrameHeader) + payload.size();
        if (would_exceed_limit(frame_bytes)) {
            return handle_limit();
        }

        auto header = make_raw_frame_header(envelope, payload.size());
        out_.write(reinterpret_cast<const char*>(&header), sizeof(header));
        if (!payload.empty()) {
            out_.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
        }
        if (!out_) {
            return {RawAppendStatus::IoError, bytes_written_, frames_written_};
        }

        bytes_written_ += frame_bytes;
        ++frames_written_;
        return {RawAppendStatus::Written, bytes_written_, frames_written_};
    }

    RawAppendResult append(const md::core::RawEnvelope& envelope, std::string_view payload) {
        const auto* data = reinterpret_cast<const std::byte*>(payload.data());
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

    [[nodiscard]] bool is_open() const noexcept {
        return out_.is_open();
    }

    [[nodiscard]] bool limited() const noexcept {
        return limited_;
    }

    [[nodiscard]] std::uint64_t bytes_written() const noexcept {
        return bytes_written_;
    }

    [[nodiscard]] std::uint64_t frames_written() const noexcept {
        return frames_written_;
    }

private:
    static std::uint64_t now_epoch_ns() noexcept {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    }

    [[nodiscard]] bool would_exceed_limit(std::uint64_t next_frame_bytes) const noexcept {
        if (options_.max_total_bytes == 0) {
            return false;
        }
        if (bytes_written_ > options_.max_total_bytes) {
            return true;
        }
        return next_frame_bytes > options_.max_total_bytes - bytes_written_;
    }

    RawAppendResult handle_limit() {
        switch (options_.limit_policy) {
        case RawStorageLimitPolicy::StopRecording:
            limited_ = true;
            return {RawAppendStatus::StoppedByLimit, bytes_written_, frames_written_};
        case RawStorageLimitPolicy::DropNew:
            return {RawAppendStatus::DroppedByLimit, bytes_written_, frames_written_};
        case RawStorageLimitPolicy::RotateDeleteOldest:
            limited_ = true;
            return {RawAppendStatus::RotateNotImplemented, bytes_written_, frames_written_};
        }
        limited_ = true;
        return {RawAppendStatus::StoppedByLimit, bytes_written_, frames_written_};
    }

    std::filesystem::path path_{};
    RawWriterOptions options_{};
    std::ofstream out_{};
    std::uint64_t bytes_written_{};
    std::uint64_t frames_written_{};
    bool limited_{};
};

} // namespace md::replay

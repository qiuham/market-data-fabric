#pragma once

#include "trading/core/header.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <type_traits>
#include <vector>

namespace md::replay {

inline constexpr std::array<char, 8> kEventJournalMagic{
    'M', 'D', 'E', 'V', 'T', '0', '1', '\0'};
inline constexpr std::array<char, 4> kEventFrameMagic{'E', 'V', 'T', 'F'};
inline constexpr std::uint16_t kEventJournalVersion{1};

#pragma pack(push, 1)
struct EventJournalHeader {
  char magic[8]{'M', 'D', 'E', 'V', 'T', '0', '1', '\0'};
  std::uint16_t version{kEventJournalVersion};
  std::uint16_t header_size{sizeof(EventJournalHeader)};
  std::uint32_t source_id{};
  std::uint32_t trading_day{};
  std::uint8_t reserved[44]{};
};

struct EventFrameHeader {
  char magic[4]{'E', 'V', 'T', 'F'};
  std::uint16_t version{kEventJournalVersion};
  std::uint16_t header_size{sizeof(EventFrameHeader)};
  std::uint16_t event_kind{};
  std::uint16_t schema_version{};
  std::uint32_t payload_size{};
  std::uint64_t partition_id{};
  std::uint64_t exchange_seq{};
  std::uint64_t event_seq{};
};
#pragma pack(pop)

static_assert(sizeof(EventJournalHeader) == 64);
static_assert(sizeof(EventFrameHeader) == 40);

// Journal 只负责顺序持久化标准事件。生产热路径应先写入预分配 SPSC，
// 再由独立线程调用本 writer，避免文件系统抖动进入行情处理线程。
class EventJournalWriter {
 public:
  EventJournalWriter(const std::filesystem::path& path,
                     std::uint32_t source_id,
                     std::uint32_t trading_day)
      : output_(path, std::ios::binary | std::ios::trunc) {
    const EventJournalHeader header{
        .source_id = source_id,
        .trading_day = trading_day,
    };
    output_.write(reinterpret_cast<const char*>(&header), sizeof(header));
  }

  template <typename Event>
  [[nodiscard]] bool append(const Event& event,
                            std::uint64_t partition_id = 0) {
    static_assert(std::is_trivially_copyable_v<Event>,
                  "标准事件必须可直接写入顺序 journal");
    const EventFrameHeader frame{
        .event_kind = static_cast<std::uint16_t>(event.header.kind),
        .schema_version = event.header.schema_version,
        .payload_size = static_cast<std::uint32_t>(sizeof(Event)),
        .partition_id = partition_id,
        .exchange_seq = event.header.exchange_seq,
        .event_seq = event.header.event_seq,
    };
    output_.write(reinterpret_cast<const char*>(&frame), sizeof(frame));
    output_.write(reinterpret_cast<const char*>(&event), sizeof(event));
    return output_.good();
  }

  [[nodiscard]] bool good() const noexcept { return output_.good(); }
  void flush() { output_.flush(); }

 private:
  std::ofstream output_;
};

struct EventJournalRecord {
  EventFrameHeader header{};
  std::vector<std::byte> payload{};

  template <typename Event>
  [[nodiscard]] bool decode(Event& out) const noexcept {
    static_assert(std::is_trivially_copyable_v<Event>);
    if (payload.size() != sizeof(Event) ||
        header.event_kind !=
            static_cast<std::uint16_t>(Event{}.header.kind)) {
      return false;
    }
    std::memcpy(&out, payload.data(), sizeof(Event));
    return true;
  }
};

class EventJournalReader {
 public:
  explicit EventJournalReader(const std::filesystem::path& path)
      : input_(path, std::ios::binary) {
    input_.read(reinterpret_cast<char*>(&file_header_), sizeof(file_header_));
    valid_ = input_.good() &&
             std::memcmp(file_header_.magic, kEventJournalMagic.data(),
                         kEventJournalMagic.size()) == 0 &&
             file_header_.version == kEventJournalVersion &&
             file_header_.header_size == sizeof(EventJournalHeader);
  }

  [[nodiscard]] bool valid() const noexcept { return valid_; }
  [[nodiscard]] const EventJournalHeader& header() const noexcept {
    return file_header_;
  }

  [[nodiscard]] bool next(EventJournalRecord& record) {
    if (!valid_) {
      return false;
    }
    EventFrameHeader frame{};
    input_.read(reinterpret_cast<char*>(&frame), sizeof(frame));
    if (input_.eof()) {
      return false;
    }
    if (!input_.good() ||
        std::memcmp(frame.magic, kEventFrameMagic.data(),
                    kEventFrameMagic.size()) != 0 ||
        frame.version != kEventJournalVersion ||
        frame.header_size != sizeof(EventFrameHeader)) {
      valid_ = false;
      return false;
    }
    record.header = frame;
    record.payload.resize(frame.payload_size);
    input_.read(reinterpret_cast<char*>(record.payload.data()),
                static_cast<std::streamsize>(record.payload.size()));
    if (!input_.good()) {
      valid_ = false;
      return false;
    }
    return true;
  }

 private:
  std::ifstream input_;
  EventJournalHeader file_header_{};
  bool valid_{};
};

}  // namespace md::replay

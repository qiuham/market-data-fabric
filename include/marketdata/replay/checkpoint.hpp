#pragma once

#include <cstdint>

namespace md::replay {

// CheckpointSequence 是恢复协议而不是存储格式：只有订单簿、阶段跟踪器和其他
// 必需下游都成功应用同一 journal 记录后，协调器才允许推进 applied_sequence。
struct CheckpointSequence {
  std::uint64_t partition_id{};
  std::uint64_t applied_sequence{};
  std::uint64_t journal_offset{};

  [[nodiscard]] constexpr std::uint64_t replay_from_sequence() const noexcept {
    return applied_sequence == UINT64_MAX ? UINT64_MAX : applied_sequence + 1;
  }

  [[nodiscard]] constexpr bool valid() const noexcept {
    return partition_id != 0 && applied_sequence != 0;
  }
};

class CheckpointCoordinator {
 public:
  explicit constexpr CheckpointCoordinator(CheckpointSequence restored = {})
      noexcept : committed_(restored) {}

  // journal 必须先持久化，再提交已应用边界；反过来会让崩溃恢复跳过事件。
  [[nodiscard]] constexpr bool commit(std::uint64_t partition_id,
                                      std::uint64_t applied_sequence,
                                      std::uint64_t durable_journal_offset)
      noexcept {
    if (partition_id == 0 || applied_sequence == 0 ||
        (committed_.valid() &&
         (partition_id != committed_.partition_id ||
          applied_sequence <= committed_.applied_sequence ||
          durable_journal_offset < committed_.journal_offset))) {
      return false;
    }
    committed_ = {partition_id, applied_sequence, durable_journal_offset};
    return true;
  }

  [[nodiscard]] constexpr const CheckpointSequence& committed() const noexcept {
    return committed_;
  }

 private:
  CheckpointSequence committed_{};
};

}  // namespace md::replay

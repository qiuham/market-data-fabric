#pragma once

#include "marketdata/mappers/map_outcome.hpp"

#include <cstdint>

namespace md::mappers {

struct SequenceStats {
  std::uint64_t accepted{};
  std::uint64_t duplicates{};
  std::uint64_t gaps{};
  std::uint64_t regressions{};
  std::uint64_t wrong_partition{};
  std::uint64_t rejected_while_stale{};
  std::uint64_t invalid{};
};

enum class SequenceMode : std::uint8_t {
  Contiguous,
  Monotonic,
};

// mapper在过滤消息类型和标的之前调用。正常路径只比较并更新定长整数状态；
// 不分配、不加锁，也不知道供应商或市场名称。每个实例只对应一条partition流。
class SequenceGate {
 public:
  explicit constexpr SequenceGate(
      SequenceMode mode = SequenceMode::Contiguous) noexcept
      : mode_(mode) {}

  [[nodiscard]] ContinuityObservation observe(
      std::uint64_t partition, std::uint64_t sequence) noexcept {
    if (partition == 0 || sequence == 0) {
      ++stats_.invalid;
      return result(ContinuityEvent::Invalid, partition, sequence);
    }
    last_received_ = sequence;
    if (state_ == ContinuityState::Uninitialized) {
      ++stats_.rejected_while_stale;
      return result(ContinuityEvent::AwaitingBaseline, partition, sequence);
    }
    if (partition != partition_) {
      state_ = ContinuityState::Stale;
      ++stats_.wrong_partition;
      return result(ContinuityEvent::WrongPartition, partition, sequence);
    }
    if (state_ == ContinuityState::Stale ||
        state_ == ContinuityState::Recovering) {
      ++stats_.rejected_while_stale;
      return result(ContinuityEvent::RejectedWhileStale, partition, sequence);
    }

    const auto expected = last_accepted_ + 1;
    if (sequence == expected ||
        (mode_ == SequenceMode::Monotonic && sequence > last_accepted_)) {
      last_accepted_ = sequence;
      ++stats_.accepted;
      return result(ContinuityEvent::Accepted, partition, sequence);
    }
    if (sequence == last_accepted_) {
      ++stats_.duplicates;
      return result(ContinuityEvent::Duplicate, partition, sequence);
    }

    state_ = ContinuityState::Stale;
    if (sequence > expected) {
      ++stats_.gaps;
      return result(ContinuityEvent::Gap, partition, sequence, expected);
    }
    ++stats_.regressions;
    return result(ContinuityEvent::Regression, partition, sequence, expected);
  }

  void begin_recovery() noexcept { state_ = ContinuityState::Recovering; }

  [[nodiscard]] ContinuityObservation observe_recovery(
      std::uint64_t partition, std::uint64_t sequence) noexcept {
    if (state_ != ContinuityState::Recovering || partition == 0 ||
        partition != partition_) {
      return result(ContinuityEvent::Invalid, partition, sequence);
    }
    const auto expected = last_accepted_ + 1;
    const bool accepted = sequence == expected ||
                          (mode_ == SequenceMode::Monotonic &&
                           sequence > last_accepted_);
    if (!accepted) {
      return result(sequence < expected ? ContinuityEvent::Regression
                                        : ContinuityEvent::Gap,
                    partition, sequence, expected);
    }
    last_accepted_ = sequence;
    return result(ContinuityEvent::Accepted, partition, sequence);
  }

  [[nodiscard]] bool complete_recovery(std::uint64_t partition,
                                       std::uint64_t sequence) noexcept {
    if (partition == 0) {
      return false;
    }
    if (state_ == ContinuityState::Recovering &&
        (partition != partition_ || sequence != last_accepted_)) {
      return false;
    }
    if (state_ != ContinuityState::Uninitialized &&
        state_ != ContinuityState::Recovering) {
      return false;
    }
    partition_ = partition;
    last_accepted_ = sequence;
    last_received_ = sequence;
    state_ = ContinuityState::Healthy;
    return true;
  }

  void reset() noexcept {
    state_ = ContinuityState::Uninitialized;
    partition_ = 0;
    last_accepted_ = 0;
    last_received_ = 0;
  }

  [[nodiscard]] ContinuityState state() const noexcept { return state_; }
  [[nodiscard]] bool trusted() const noexcept {
    return state_ == ContinuityState::Healthy;
  }
  [[nodiscard]] std::uint64_t partition() const noexcept {
    return partition_;
  }
  [[nodiscard]] std::uint64_t last_accepted() const noexcept {
    return last_accepted_;
  }
  [[nodiscard]] std::uint64_t last_received() const noexcept {
    return last_received_;
  }
  [[nodiscard]] const SequenceStats& stats() const noexcept { return stats_; }

 private:
  [[nodiscard]] ContinuityObservation result(
      ContinuityEvent event, std::uint64_t partition,
      std::uint64_t received, std::uint64_t expected = 0) const noexcept {
    if (expected == 0 && last_accepted_ != 0) {
      expected = last_accepted_ + 1;
    }
    return ContinuityObservation{event, state_, partition, expected, received};
  }

  ContinuityState state_{ContinuityState::Uninitialized};
  SequenceMode mode_{SequenceMode::Contiguous};
  std::uint64_t partition_{};
  std::uint64_t last_accepted_{};
  std::uint64_t last_received_{};
  SequenceStats stats_{};
};

}  // namespace md::mappers

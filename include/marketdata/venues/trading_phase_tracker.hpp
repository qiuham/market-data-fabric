#pragma once

#include "trading/events/status.hpp"

#include <cstdint>

namespace md::venues {

enum class PhaseApplyStatus : std::uint8_t {
  Applied,
  Duplicate,
  Regression,
  Unsupported,
};

// 每个标的由 router 持有一个实例。它只理解标准 Status，不依赖通联字符串，
// 因而 SSE 的其他数据源以及后续其他市场均可复用同一状态容器。
class TradingPhaseTracker {
 public:
  [[nodiscard]] PhaseApplyStatus apply(
      const trading::events::Status& event) noexcept {
    if (event.status_type != trading::core::StatusType::TradingPhase ||
        event.trading_phase == 0 || event.header.exchange_seq == 0) {
      return PhaseApplyStatus::Unsupported;
    }
    if (event.header.exchange_seq == sequence_) {
      return PhaseApplyStatus::Duplicate;
    }
    if (event.header.exchange_seq < sequence_) {
      return PhaseApplyStatus::Regression;
    }
    phase_ = event.trading_phase;
    sequence_ = event.header.exchange_seq;
    return PhaseApplyStatus::Applied;
  }

  [[nodiscard]] std::uint16_t phase() const noexcept { return phase_; }
  [[nodiscard]] std::uint64_t sequence() const noexcept { return sequence_; }

 private:
  std::uint16_t phase_{};
  std::uint64_t sequence_{};
};

}  // namespace md::venues

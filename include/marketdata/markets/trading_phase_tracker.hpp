#pragma once

#include "trading/events/trading_phase.hpp"

#include <cstdint>

namespace md::markets {

enum class PhaseApplyStatus : std::uint8_t {
  Applied,
  Duplicate,
  Regression,
  Unsupported,
};

// 每个标的由 router 持有一个实例。它只理解标准 TradingPhaseUpdate，不依赖通联字符串，
// 因而 SSE 的其他数据源以及后续其他市场均可复用同一状态容器。
class TradingPhaseTracker {
 public:
  [[nodiscard]] PhaseApplyStatus apply(
      const trading::events::TradingPhaseUpdate& event) noexcept {
    if (event.trading_phase == trading::core::TradingPhase::Unknown ||
        event.header.exchange_seq == 0) {
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

  [[nodiscard]] trading::core::TradingPhase phase() const noexcept {
    return phase_;
  }
  [[nodiscard]] std::uint64_t sequence() const noexcept { return sequence_; }

 private:
  trading::core::TradingPhase phase_{trading::core::TradingPhase::Unknown};
  std::uint64_t sequence_{};
};

}  // namespace md::markets

#pragma once

#include <cstdint>

namespace md::adapters {

enum class MapStatus : std::uint8_t {
  Mapped,
  MappedDiagnostic,
  Ignored,
  Invalid,
  Unsupported,
};

struct MapResult {
  MapStatus status{MapStatus::Invalid};
  bool lossless{};

  [[nodiscard]] constexpr bool ok() const noexcept {
    return status == MapStatus::Mapped ||
           status == MapStatus::MappedDiagnostic;
  }
};

enum class ContinuityState : std::uint8_t {
  Uninitialized,
  Healthy,
  Stale,
  Recovering,
};

enum class ContinuityEvent : std::uint8_t {
  Accepted,
  Duplicate,
  Gap,
  Regression,
  WrongPartition,
  AwaitingBaseline,
  RejectedWhileStale,
  Invalid,
};

struct ContinuityObservation {
  ContinuityEvent event{ContinuityEvent::Invalid};
  ContinuityState state{ContinuityState::Uninitialized};
  std::uint64_t partition{};
  std::uint64_t expected{};
  std::uint64_t received{};

  [[nodiscard]] constexpr bool accepted() const noexcept {
    return event == ContinuityEvent::Accepted;
  }

  [[nodiscard]] constexpr bool trusted() const noexcept {
    return state == ContinuityState::Healthy;
  }
};

struct MapOutcome {
  MapResult mapping{};
  ContinuityObservation continuity{};

  [[nodiscard]] constexpr bool publishable() const noexcept {
    return continuity.accepted() && mapping.ok();
  }
};

}  // namespace md::adapters

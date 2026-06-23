#pragma once

#include "md/core/message.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace md::core {

using EventType = PayloadKind;

enum class Side : std::uint8_t {
  Unknown = 0,
  Buy = 1,
  Sell = 2,
};

enum class BookAction : std::uint8_t {
  Add = 1,
  Update = 2,
  Delete = 3,
};

enum class OrderAction : std::uint8_t {
  Unknown = 0,
  Add = 1,
  Modify = 2,
  Cancel = 3,
  Delete = 4,
  Replace = 5,
};

enum class OrderType : std::uint8_t {
  Unknown = 0,
  Limit = 1,
  Market = 2,
  Best = 3,
  BestFive = 4,
  ImmediateOrCancel = 5,
  FillOrKill = 6,
};

enum class TimeInForce : std::uint8_t {
  Unknown = 0,
  Day = 1,
  GoodTillCancel = 2,
  ImmediateOrCancel = 3,
  FillOrKill = 4,
  Auction = 5,
};

enum class ExecutionType : std::uint8_t {
  Unknown = 0,
  Trade = 1,
  Cancel = 2,
  Correct = 3,
  Bust = 4,
};

enum class AggressorSide : std::uint8_t {
  Unknown = 0,
  Buy = 1,
  Sell = 2,
  None = 3,
};

using MdHeader = MessageEnvelope;

struct Trade {
  MdHeader header{.payload_kind = PayloadKind::Trade,
                  .payload_encoding = PayloadEncoding::MdStruct};
  std::int64_t price{};
  std::int64_t quantity{};
  std::uint64_t trade_id{};
  // Taker/aggressor side. Provider maker-side fields must be inverted by mapper.
  AggressorSide aggressor_side{AggressorSide::Unknown};
  std::uint8_t condition{};
};

struct Quote {
  MdHeader header{.payload_kind = PayloadKind::Quote,
                  .payload_encoding = PayloadEncoding::MdStruct};
  std::int64_t bid_price{};
  std::int64_t bid_quantity{};
  std::int64_t ask_price{};
  std::int64_t ask_quantity{};
};

struct BookLevel {
  std::int64_t price{};
  std::int64_t quantity{};
  std::uint32_t order_count{};
};

enum class BookUpdateType : std::uint8_t {
  Unknown = 0,
  Snapshot = 1,
  Delta = 2,
  Heartbeat = 3,
  Reset = 4,
};

enum class BookUpdateFlag : std::uint32_t {
  None = 0,
  ChecksumPresent = 1u << 0,
  PrevSeqPresent = 1u << 1,
  GapDetected = 1u << 2,
};

constexpr std::uint32_t book_update_flag(BookUpdateFlag flag) noexcept {
  return static_cast<std::uint32_t>(flag);
}

constexpr std::uint32_t operator|(BookUpdateFlag lhs,
                                  BookUpdateFlag rhs) noexcept {
  return book_update_flag(lhs) | book_update_flag(rhs);
}

struct BookLevelUpdate {
  Side side{Side::Unknown};
  BookAction action{BookAction::Update};
  std::uint16_t level{};
  std::int64_t price{};
  std::int64_t quantity{};
  std::uint32_t order_count{};
};

template <std::size_t MaxLevels = 128> struct BookUpdate {
  static_assert(MaxLevels <= 65535,
                "BookUpdate level count must fit in uint16_t");

  MdHeader header{.payload_kind = PayloadKind::BookUpdate,
                  .payload_encoding = PayloadEncoding::MdStruct};
  BookUpdateType update_type{BookUpdateType::Delta};
  std::uint64_t first_exchange_seq{};
  std::uint64_t last_exchange_seq{};
  std::uint64_t prev_exchange_seq{};
  std::int32_t checksum{};
  std::uint32_t flags{};
  std::uint16_t level_count{};
  std::array<BookLevelUpdate, MaxLevels> levels{};

  [[nodiscard]] static constexpr std::size_t capacity() noexcept {
    return MaxLevels;
  }

  [[nodiscard]] bool add_level(const BookLevelUpdate &level) noexcept {
    if (level_count >= MaxLevels) {
      return false;
    }
    levels[level_count++] = level;
    return true;
  }

  [[nodiscard]] bool add_level(Side side, BookAction action,
                               std::int64_t price,
                               std::int64_t quantity) noexcept {
    return add_level(BookLevelUpdate{.side = side,
                                     .action = action,
                                     .level = 0,
                                     .price = price,
                                     .quantity = quantity,
                                     .order_count = 0});
  }

  void clear_levels() noexcept { level_count = 0; }
};

// Compatibility helper for single-level producers; provider depth messages should
// use BookUpdate so sequence/checksum metadata stays at message scope.
struct BookDelta {
  MdHeader header{.payload_kind = PayloadKind::BookUpdate,
                  .payload_encoding = PayloadEncoding::MdStruct};
  Side side{Side::Unknown};
  BookAction action{BookAction::Update};
  std::uint16_t level{};
  std::int64_t price{};
  std::int64_t quantity{};
  std::uint64_t order_id{};
};

template <std::size_t Depth = 20> struct BookSnapshot {
  MdHeader header{.payload_kind = PayloadKind::BookSnapshot,
                  .payload_encoding = PayloadEncoding::MdStruct};
  std::array<BookLevel, Depth> bids{};
  std::array<BookLevel, Depth> asks{};
  std::uint16_t bid_count{};
  std::uint16_t ask_count{};
  std::int32_t checksum{};
};

struct OrderEvent {
  MdHeader header{.payload_kind = PayloadKind::OrderEvent,
                  .payload_encoding = PayloadEncoding::MdStruct};
  std::uint64_t order_id{};
  std::uint64_t original_order_id{};
  std::uint64_t partition_id{};
  std::uint64_t order_seq{};
  std::uint64_t priority_id{};
  std::int64_t price{};
  std::int64_t quantity{};
  std::int64_t remaining_quantity{};
  std::int64_t display_quantity{};
  Side side{Side::Unknown};
  OrderAction action{OrderAction::Unknown};
  OrderType order_type{OrderType::Unknown};
  TimeInForce time_in_force{TimeInForce::Unknown};
  std::uint16_t condition{};
  std::uint16_t trading_phase{};
};

struct Execution {
  MdHeader header{.payload_kind = PayloadKind::Execution,
                  .payload_encoding = PayloadEncoding::MdStruct};
  std::uint64_t trade_id{};
  std::uint64_t partition_id{};
  std::uint64_t execution_seq{};
  std::uint64_t buy_order_id{};
  std::uint64_t sell_order_id{};
  std::uint64_t resting_order_id{};
  std::uint64_t aggressor_order_id{};
  std::int64_t price{};
  std::int64_t quantity{};
  ExecutionType execution_type{ExecutionType::Unknown};
  AggressorSide aggressor_side{AggressorSide::Unknown};
  std::uint16_t condition{};
  std::uint16_t trading_phase{};
};

struct Status {
  MdHeader header{.payload_kind = PayloadKind::Status,
                  .payload_encoding = PayloadEncoding::MdStruct};
  std::uint16_t status_type{};
  std::uint16_t trading_status{};
  std::uint32_t reason_code{};
};

} // namespace md::core

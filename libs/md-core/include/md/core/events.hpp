#pragma once

#include "md/core/message.hpp"

#include <array>
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
  Side side{Side::Unknown};
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

struct BookDelta {
  MdHeader header{.payload_kind = PayloadKind::BookDelta,
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
  std::uint8_t bid_count{};
  std::uint8_t ask_count{};
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

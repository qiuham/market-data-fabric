#pragma once

#include "trading/core/header.hpp"

#include <cstdint>

namespace md::venues::cn {

namespace tc = trading::core;

struct EventContext {
  tc::SourceId source_id{};
  tc::VenueId venue_id{};
  tc::FeedId feed_id{};
  tc::InstrumentId instrument_id{};
  tc::TradingDay trading_day{};
};

enum class OrderKind : std::uint8_t {
  Unknown,
  AddLimit,
  AddMarket,
  AddOwnBest,
  Delete,
  Status,
};

// 跨供应商稳定的中国证券交易阶段编码。供应商字符串只允许在 adapter 中出现，
// 下游 phase tracker 和 journal 只消费这里的市场语义。
enum class TradingPhase : std::uint16_t {
  Unknown = 0,
  OpeningCall = 1,
  Continuous = 2,
  ClosingCall = 3,
  Closed = 4,
  EndOfTrading = 5,
};

struct OrderView {
  tc::TimestampNs exchange_ts_ns{};
  tc::Sequence event_seq{};
  tc::Sequence exchange_seq{};
  tc::PartitionId partition_id{};
  tc::OrderId order_id{};
  tc::OrderId original_order_id{};
  tc::Price price{};
  tc::Quantity quantity{};
  tc::Side side{tc::Side::Unknown};
  OrderKind kind{OrderKind::Unknown};
  TradingPhase trading_phase{TradingPhase::Unknown};
};

enum class TransactionKind : std::uint8_t {
  Unknown,
  Trade,
  Cancel,
};

struct TransactionView {
  tc::TimestampNs exchange_ts_ns{};
  tc::Sequence event_seq{};
  tc::Sequence exchange_seq{};
  tc::PartitionId partition_id{};
  tc::TradeId trade_id{};
  tc::OrderId bid_order_id{};
  tc::OrderId ask_order_id{};
  tc::Price price{};
  tc::Quantity quantity{};
  tc::AggressorSide aggressor_side{tc::AggressorSide::Unknown};
  bool neutral_trade{};
  TransactionKind kind{TransactionKind::Unknown};
};

inline void fill_header(const EventContext& context, tc::EventKind kind,
                        tc::TimestampNs exchange_ts_ns,
                        tc::Sequence event_seq,
                        tc::Sequence exchange_seq,
                        tc::EventHeader& out) noexcept {
  out = {};
  out.kind = kind;
  out.source_id = context.source_id;
  out.venue_id = context.venue_id;
  out.feed_id = context.feed_id;
  out.instrument_id = context.instrument_id;
  out.trading_day = context.trading_day;
  out.event_seq = event_seq;
  out.exchange_seq = exchange_seq;
  out.exchange_ts_ns = exchange_ts_ns;
}

}  // namespace md::venues::cn

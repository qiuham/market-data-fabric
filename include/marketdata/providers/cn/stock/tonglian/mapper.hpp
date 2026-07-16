#pragma once

#include "marketdata/mappers/sequence_gate.hpp"
#include "marketdata/markets/cn/sse/mapper.hpp"
#include "marketdata/markets/cn/szse/mapper.hpp"
#include "trading/events/order_book.hpp"
#include "trading/events/trading_phase.hpp"

#include <cstdint>

namespace md::providers::tonglian {

namespace tc = trading::core;
namespace te = trading::events;

inline constexpr tc::SourceId kCnStockTonglianSourceId = 1001;

enum class Market : std::uint8_t {
  Unknown = 0,
  Shanghai = 1,
  Shenzhen = 2,
};

using md::mappers::ContinuityEvent;
using md::mappers::ContinuityObservation;
using md::mappers::ContinuityState;
using md::mappers::MapOutcome;
using md::mappers::MapResult;
using md::mappers::MapStatus;

struct MappingContext {
  tc::SourceId source_id{kCnStockTonglianSourceId};
  tc::VenueId venue_id{};
  tc::FeedId feed_id{};
  tc::InstrumentId instrument_id{};
  tc::TradingDay trading_day{};
  Market market{Market::Unknown};
};

struct OrderRow {
  std::uint32_t time_hhmmssmmm{};
  tc::Sequence row_seq{};
  tc::OrderId order_id{};
  tc::OrderId exchange_order_id{};
  char order_kind{};
  char function_code{};
  tc::Price price{};
  tc::Quantity quantity{};
  tc::PartitionId channel{};
  tc::OrderId original_order_id{};
  tc::Sequence biz_index{};
  md::markets::cn::TradingPhase trading_phase{
      md::markets::cn::TradingPhase::Unknown};
};

struct TransactionRow {
  std::uint32_t time_hhmmssmmm{};
  tc::Sequence row_seq{};
  tc::TradeId transaction_id{};
  char function_code{};
  char order_kind{};
  char bs_flag{};
  tc::Price price{};
  tc::Quantity quantity{};
  tc::OrderId ask_order_id{};
  tc::OrderId bid_order_id{};
  tc::PartitionId channel{};
  tc::Sequence biz_index{};
};

struct OrderMapOutput {
  tc::EventKind event_kind{tc::EventKind::Unknown};
  te::BookOrder order{};
  te::TradingPhaseUpdate phase_update{};
};

// 通联把成交与撤单混放在逐笔成交流中；统一输出在 mapper 边界拆成订单生命周期
// 或订单关联成交，避免把供应商文件分类泄漏到 trading-core。
struct TransactionMapOutput {
  tc::EventKind event_kind{tc::EventKind::Unknown};
  te::BookOrder order{};
  te::BookTrade trade{};
};

[[nodiscard]] constexpr tc::TimestampNs hhmmssmmm_to_ns(
    std::uint32_t value) noexcept {
  const auto milliseconds = value % 1000u;
  value /= 1000u;
  const auto seconds = value % 100u;
  value /= 100u;
  const auto minutes = value % 100u;
  value /= 100u;
  const auto hours = value;
  return (((static_cast<tc::TimestampNs>(hours) * 60u + minutes) * 60u +
           seconds) *
              1000u +
          milliseconds) *
         1'000'000u;
}

[[nodiscard]] constexpr tc::Side map_side(char function_code) noexcept {
  switch (function_code) {
    case 'B':
      return tc::Side::Buy;
    case 'S':
      return tc::Side::Sell;
    default:
      return tc::Side::Unknown;
  }
}

[[nodiscard]] constexpr tc::AggressorSide map_aggressor_side(
    char bs_flag) noexcept {
  switch (bs_flag) {
    case 'B':
      return tc::AggressorSide::Buy;
    case 'S':
      return tc::AggressorSide::Sell;
    default:
      return tc::AggressorSide::Unknown;
  }
}

[[nodiscard]] constexpr tc::OrderId effective_order_id(
    const OrderRow& row) noexcept {
  // 通联此处的 order_id 是通道代码，真正用于订单簿定位的是交易所订单号。
  return row.exchange_order_id;
}

[[nodiscard]] inline MapResult map_order_row(const MappingContext& context,
                                             const OrderRow& row,
                                             te::BookOrder& out) noexcept {
  namespace cn = md::markets::cn;
  cn::OrderKind kind{cn::OrderKind::Unknown};
  if (context.market == Market::Shanghai) {
    kind = row.order_kind == 'A'   ? cn::OrderKind::AddLimit
           : row.order_kind == 'D' ? cn::OrderKind::Delete
           : row.order_kind == 'S' ? cn::OrderKind::TradingPhase
                                   : cn::OrderKind::Unknown;
  } else if (context.market == Market::Shenzhen) {
    kind = row.order_kind == '0'   ? cn::OrderKind::AddLimit
           : row.order_kind == '1' ? cn::OrderKind::AddMarket
           : row.order_kind == 'U' ? cn::OrderKind::AddOwnBest
                                   : cn::OrderKind::Unknown;
  }
  const cn::EventContext event_context{
      context.source_id, context.venue_id, context.feed_id,
      context.instrument_id, context.trading_day};
  const cn::OrderView view{
      .exchange_ts_ns = hhmmssmmm_to_ns(row.time_hhmmssmmm),
      .event_seq = row.row_seq,
      .exchange_seq = row.biz_index,
      .partition_id = row.channel,
      .order_id = effective_order_id(row),
      .original_order_id = row.original_order_id,
      .price = row.price,
      .quantity = row.quantity,
      .side = map_side(row.function_code),
      .kind = kind,
      .trading_phase = row.trading_phase,
  };
  if (context.market == Market::Shanghai) {
    return md::markets::cn::sse::map(event_context, view, out);
  }
  if (context.market == Market::Shenzhen) {
    return md::markets::cn::szse::map(event_context, view, out);
  }
  return {MapStatus::Unsupported, false};
}

[[nodiscard]] inline MapResult map_status_row(
    const MappingContext& context, const OrderRow& row,
    te::TradingPhaseUpdate& out) noexcept {
  if (context.market != Market::Shanghai || row.order_kind != 'S') {
    return {MapStatus::Unsupported, false};
  }
  const md::markets::cn::EventContext event_context{
      context.source_id, context.venue_id, context.feed_id,
      context.instrument_id, context.trading_day};
  const md::markets::cn::OrderView view{
      .exchange_ts_ns = hhmmssmmm_to_ns(row.time_hhmmssmmm),
      .event_seq = row.row_seq,
      .exchange_seq = row.biz_index,
      .partition_id = row.channel,
      .kind = md::markets::cn::OrderKind::TradingPhase,
      .trading_phase = row.trading_phase,
  };
  return md::markets::cn::sse::map(event_context, view, out);
}

[[nodiscard]] inline MapResult map_transaction_row(
    const MappingContext& context, const TransactionRow& row,
    TransactionMapOutput& out) noexcept {
  namespace cn = md::markets::cn;
  const auto exchange_seq =
      row.biz_index != 0 ? row.biz_index : row.transaction_id;
  const auto kind = row.function_code == 'C'
                        ? cn::TransactionKind::Cancel
                        : (row.function_code == '0' ||
                                   row.function_code == '\0'
                               ? cn::TransactionKind::Trade
                               : cn::TransactionKind::Unknown);
  const cn::EventContext event_context{
      context.source_id, context.venue_id, context.feed_id,
      context.instrument_id, context.trading_day};
  const cn::TransactionView view{
      .exchange_ts_ns = hhmmssmmm_to_ns(row.time_hhmmssmmm),
      .event_seq = row.row_seq,
      .exchange_seq = exchange_seq,
      .partition_id = row.channel,
      .trade_id = row.transaction_id,
      .bid_order_id = row.bid_order_id,
      .ask_order_id = row.ask_order_id,
      .price = row.price,
      .quantity = row.quantity,
      .aggressor_side = map_aggressor_side(row.bs_flag),
      .neutral_trade = row.bs_flag == 'N',
      .kind = kind,
  };
  if (context.market == Market::Shanghai) {
    if (kind == cn::TransactionKind::Cancel) {
      const auto result =
          md::markets::cn::sse::map(event_context, view, out.order);
      out.event_kind = result.ok() ? tc::EventKind::BookOrder
                                   : tc::EventKind::Unknown;
      return result;
    }
    const auto result =
        md::markets::cn::sse::map(event_context, view, out.trade);
    out.event_kind = result.ok() ? tc::EventKind::BookTrade
                                 : tc::EventKind::Unknown;
    return result;
  }
  if (context.market == Market::Shenzhen) {
    if (kind == cn::TransactionKind::Cancel) {
      const auto result =
          md::markets::cn::szse::map(event_context, view, out.order);
      out.event_kind = result.ok() ? tc::EventKind::BookOrder
                                   : tc::EventKind::Unknown;
      return result;
    }
    const auto result =
        md::markets::cn::szse::map(event_context, view, out.trade);
    out.event_kind = result.ok() ? tc::EventKind::BookTrade
                                 : tc::EventKind::Unknown;
    return result;
  }
  return {MapStatus::Unsupported, false};
}

// 实盘和回放共用这个有状态入口：每条原始记录只检查一次连续性，随后由同一个
// MapOutcome 决定是否允许发布标准事件，避免序号检查与字段映射产生分叉。
class TonglianMapper {
 public:
  explicit TonglianMapper(MappingContext context) noexcept
      : context_(context) {}

  void begin_recovery() noexcept { sequence_.begin_recovery(); }

  [[nodiscard]] bool complete_recovery(tc::PartitionId channel,
                                       tc::Sequence sequence) noexcept {
    return sequence_.complete_recovery(channel, sequence);
  }

  void reset() noexcept { sequence_.reset(); }

  // 一条通联委托流记录可能产生盘口事件或阶段事件。唯一输出包装消除调用方
  // 误用“只映射委托”重载而静默丢状态的可能，仍然不需要堆分配或虚调用。
  [[nodiscard]] MapOutcome map(const OrderRow& row,
                               OrderMapOutput& out) noexcept {
    out.event_kind = tc::EventKind::Unknown;
    const auto continuity = sequence_.observe(row.channel, row.biz_index);
    if (!continuity.accepted()) {
      return {{MapStatus::Ignored, false}, continuity};
    }
    if (row.order_kind == 'S') {
      const auto mapped = map_status_row(context_, row, out.phase_update);
      out.event_kind = mapped.ok() ? tc::EventKind::TradingPhaseUpdate
                                   : tc::EventKind::Unknown;
      return {mapped, continuity};
    }
    const auto mapped = map_order_row(context_, row, out.order);
    out.event_kind = mapped.ok() ? tc::EventKind::BookOrder
                                 : tc::EventKind::Unknown;
    return {mapped, continuity};
  }

  [[nodiscard]] MapOutcome map(const TransactionRow& row,
                               TransactionMapOutput& out) noexcept {
    out.event_kind = tc::EventKind::Unknown;
    const auto continuity = sequence_.observe(row.channel, row.biz_index);
    if (!continuity.accepted()) {
      return {{MapStatus::Ignored, false}, continuity};
    }
    return {map_transaction_row(context_, row, out), continuity};
  }

  [[nodiscard]] bool stream_trusted() const noexcept {
    return sequence_.trusted();
  }

  [[nodiscard]] const md::mappers::SequenceStats&
  sequence_stats() const noexcept {
    return sequence_.stats();
  }

 private:
  MappingContext context_{};
  md::mappers::SequenceGate sequence_{};
};

}  // namespace md::providers::tonglian

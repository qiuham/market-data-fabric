#pragma once

#include "marketdata/mappers/map_outcome.hpp"
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

struct SequenceStats {
  std::uint64_t accepted{};
  std::uint64_t duplicates{};
  std::uint64_t gaps{};
  std::uint64_t regressions{};
  std::uint64_t wrong_channel{};
  std::uint64_t rejected_while_stale{};
  std::uint64_t invalid{};
};

// 序号检查必须早于消息类型和证券代码过滤；即使某条状态消息不进入订单簿，
// 它仍然占用交易所序号，必须推进当前 Channel 的连续性边界。
class TonglianSequenceGate {
 public:
  [[nodiscard]] ContinuityObservation observe(
      std::uint64_t channel, std::uint64_t sequence) noexcept {
    if (channel == 0 || sequence == 0) {
      ++stats_.invalid;
      return result(ContinuityEvent::Invalid, channel, sequence);
    }
    last_received_ = sequence;
    if (state_ == ContinuityState::Uninitialized) {
      ++stats_.rejected_while_stale;
      return result(ContinuityEvent::AwaitingBaseline, channel, sequence);
    }
    if (channel != channel_) {
      state_ = ContinuityState::Stale;
      ++stats_.wrong_channel;
      return result(ContinuityEvent::WrongPartition, channel, sequence);
    }
    if (state_ == ContinuityState::Stale ||
        state_ == ContinuityState::Recovering) {
      ++stats_.rejected_while_stale;
      return result(ContinuityEvent::RejectedWhileStale, channel, sequence);
    }

    const auto expected = last_accepted_ + 1;
    if (sequence == expected) {
      last_accepted_ = sequence;
      ++stats_.accepted;
      return result(ContinuityEvent::Accepted, channel, sequence);
    }
    if (sequence == last_accepted_) {
      ++stats_.duplicates;
      return result(ContinuityEvent::Duplicate, channel, sequence);
    }

    state_ = ContinuityState::Stale;
    if (sequence > expected) {
      ++stats_.gaps;
      return result(ContinuityEvent::Gap, channel, sequence, expected);
    }
    ++stats_.regressions;
    return result(ContinuityEvent::Regression, channel, sequence, expected);
  }

  void begin_recovery() noexcept { state_ = ContinuityState::Recovering; }

  // 恢复流必须逐条连续推进，不能用一个较大的尾序号直接“宣告恢复”，否则缺口
  // 中间是否真正补齐无法证明。返回 Accepted 的记录才允许送往下游重放。
  [[nodiscard]] ContinuityObservation observe_recovery(
      std::uint64_t channel, std::uint64_t sequence) noexcept {
    if (state_ != ContinuityState::Recovering || channel == 0 ||
        channel != channel_) {
      return result(ContinuityEvent::Invalid, channel, sequence);
    }
    const auto expected = last_accepted_ + 1;
    if (sequence != expected) {
      return result(sequence < expected ? ContinuityEvent::Regression
                                        : ContinuityEvent::Gap,
                    channel, sequence, expected);
    }
    last_accepted_ = sequence;
    return result(ContinuityEvent::Accepted, channel, sequence);
  }

  // Uninitialized 时用于 checkpoint/replay 建立可信基线；Recovering 时只允许在
  // 调用方声明的已重放尾序号与内部逐条推进位置完全一致后恢复实时发布。
  [[nodiscard]] bool complete_recovery(std::uint64_t channel,
                                       std::uint64_t sequence) noexcept {
    if (channel == 0) {
      return false;
    }
    if (state_ == ContinuityState::Recovering &&
        (channel != channel_ || sequence != last_accepted_)) {
      return false;
    }
    if (state_ != ContinuityState::Uninitialized &&
        state_ != ContinuityState::Recovering) {
      return false;
    }
    channel_ = channel;
    last_accepted_ = sequence;
    last_received_ = sequence;
    state_ = ContinuityState::Healthy;
    return true;
  }

  void reset() noexcept {
    state_ = ContinuityState::Uninitialized;
    channel_ = 0;
    last_accepted_ = 0;
    last_received_ = 0;
  }

  [[nodiscard]] ContinuityState state() const noexcept { return state_; }
  [[nodiscard]] bool book_trusted() const noexcept {
    return state_ == ContinuityState::Healthy;
  }
  [[nodiscard]] std::uint64_t channel() const noexcept { return channel_; }
  [[nodiscard]] std::uint64_t last_accepted() const noexcept {
    return last_accepted_;
  }
  [[nodiscard]] std::uint64_t last_received() const noexcept {
    return last_received_;
  }
  [[nodiscard]] const SequenceStats& stats() const noexcept { return stats_; }

 private:
  [[nodiscard]] ContinuityObservation result(
      ContinuityEvent event, std::uint64_t channel, std::uint64_t received,
      std::uint64_t expected = 0) const noexcept {
    if (expected == 0 && last_accepted_ != 0) {
      expected = last_accepted_ + 1;
    }
    return ContinuityObservation{event, state_, channel, expected, received};
  }

  ContinuityState state_{ContinuityState::Uninitialized};
  std::uint64_t channel_{};
  std::uint64_t last_accepted_{};
  std::uint64_t last_received_{};
  SequenceStats stats_{};
};

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
    return sequence_.book_trusted();
  }

  [[nodiscard]] const SequenceStats& sequence_stats() const noexcept {
    return sequence_.stats();
  }

 private:
  MappingContext context_{};
  TonglianSequenceGate sequence_{};
};

}  // namespace md::providers::tonglian

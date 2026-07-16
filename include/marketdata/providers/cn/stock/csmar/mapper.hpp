#pragma once

#include "marketdata/mappers/sequence_gate.hpp"
#include "marketdata/markets/cn/sse/mapper.hpp"
#include "marketdata/markets/cn/szse/mapper.hpp"
#include "trading/events/order_book.hpp"
#include "trading/events/trading_phase.hpp"

#include <cstdint>

namespace md::providers::csmar {

namespace tc = trading::core;
namespace te = trading::events;

inline constexpr tc::SourceId kCnStockCsmarSourceId = 1002;

enum class Market : std::uint8_t {
  Unknown,
  Shanghai,
  Shenzhen,
};

struct MappingContext {
  tc::SourceId source_id{kCnStockCsmarSourceId};
  tc::VenueId venue_id{};
  tc::FeedId feed_id{};
  tc::InstrumentId instrument_id{};
  tc::TradingDay trading_day{};
  Market market{Market::Unknown};
};

// CSV读取只负责把十进制价格和数量转换成refdata规定的整数比例尺。mapper不解析
// 字符串，也不把离线文件格式泄漏到统一事件。
struct OrderRow {
  tc::TimestampNs exchange_time_ns{};
  tc::Sequence record_id{};
  tc::PartitionId set_id{};
  tc::OrderId order_id{};
  tc::Price price{};
  tc::Quantity quantity{};
  tc::Side side{tc::Side::Unknown};
  char order_type{};  // A新增、D撤单、S交易阶段。
  tc::TradingPhase trading_phase{tc::TradingPhase::Unknown};
};

struct TradeRow {
  tc::TimestampNs exchange_time_ns{};
  tc::Sequence record_id{};
  tc::PartitionId channel{};
  tc::TradeId trade_id{};
  tc::OrderId buy_order_id{};
  tc::OrderId sell_order_id{};
  tc::Price price{};
  tc::Quantity quantity{};
  tc::AggressorSide aggressor_side{tc::AggressorSide::Unknown};
};

struct OrderMapOutput {
  tc::EventKind event_kind{tc::EventKind::Unknown};
  te::BookOrder order{};
  te::TradingPhaseUpdate phase{};
};

class CsmarMapper {
 public:
  explicit CsmarMapper(MappingContext context) noexcept
      : context_(context), sequence_(md::mappers::SequenceMode::Monotonic) {}

  [[nodiscard]] bool establish_baseline(tc::PartitionId partition,
                                        tc::Sequence sequence) noexcept {
    return sequence_.complete_recovery(partition, sequence);
  }

  [[nodiscard]] md::mappers::MapOutcome map(
      const OrderRow& row, OrderMapOutput& out) noexcept {
    out.event_kind = tc::EventKind::Unknown;
    const auto continuity = sequence_.observe(row.set_id, row.record_id);
    if (!continuity.accepted()) {
      return {{md::mappers::MapStatus::Ignored, false}, continuity};
    }

    if (row.order_type == 'S') {
      const md::markets::cn::OrderView view{
          .exchange_ts_ns = row.exchange_time_ns,
          .event_seq = row.record_id,
          .exchange_seq = row.record_id,
          .partition_id = row.set_id,
          .kind = md::markets::cn::OrderKind::TradingPhase,
          .trading_phase = row.trading_phase,
      };
      const auto mapped = map_phase(view, out.phase);
      out.event_kind = mapped.ok() ? tc::EventKind::TradingPhaseUpdate
                                   : tc::EventKind::Unknown;
      return {mapped.ok()
                  ? md::mappers::MapResult{md::mappers::MapStatus::Mapped,
                                           true}
                  : mapped,
              continuity};
    }

    if (row.order_type == 'D') {
      const md::markets::cn::TransactionView view{
          .exchange_ts_ns = row.exchange_time_ns,
          .event_seq = row.record_id,
          .exchange_seq = row.record_id,
          .partition_id = row.set_id,
          .bid_order_id = row.side == tc::Side::Buy ? row.order_id : 0,
          .ask_order_id = row.side == tc::Side::Sell ? row.order_id : 0,
          .quantity = row.quantity,
          .kind = md::markets::cn::TransactionKind::Cancel,
      };
      const auto mapped = map_cancel(view, out.order);
      out.event_kind = mapped.ok() ? tc::EventKind::BookOrder
                                   : tc::EventKind::Unknown;
      return {mapped.ok()
                  ? md::mappers::MapResult{md::mappers::MapStatus::Mapped,
                                           true}
                  : mapped,
              continuity};
    }

    const md::markets::cn::OrderView view{
        .exchange_ts_ns = row.exchange_time_ns,
        .event_seq = row.record_id,
        .exchange_seq = row.record_id,
        .partition_id = row.set_id,
        .order_id = row.order_id,
        .price = row.price,
        .quantity = row.quantity,
        .side = row.side,
        .kind = row.order_type == 'A'
                    ? md::markets::cn::OrderKind::AddLimit
                    : md::markets::cn::OrderKind::Unknown,
    };
    const auto mapped = map_order(view, out.order);
    out.event_kind = mapped.ok() ? tc::EventKind::BookOrder
                                 : tc::EventKind::Unknown;
    return {mapped.ok()
                ? md::mappers::MapResult{md::mappers::MapStatus::Mapped, true}
                : mapped,
            continuity};
  }

  [[nodiscard]] md::mappers::MapOutcome map(
      const TradeRow& row, te::BookTrade& out) noexcept {
    const auto continuity = sequence_.observe(row.channel, row.record_id);
    if (!continuity.accepted()) {
      return {{md::mappers::MapStatus::Ignored, false}, continuity};
    }
    const auto trade_id = row.trade_id != 0 ? row.trade_id : row.record_id;
    if (context_.market == Market::Unknown || trade_id == 0 ||
        row.quantity <= 0 || row.price < 0 ||
        (row.buy_order_id == 0 && row.sell_order_id == 0)) {
      return {{md::mappers::MapStatus::Invalid, false}, continuity};
    }

    const md::markets::cn::TransactionView view{
        .exchange_ts_ns = row.exchange_time_ns,
        .event_seq = row.record_id,
        .exchange_seq = row.record_id,
        .partition_id = row.channel,
        .trade_id = trade_id,
        .bid_order_id = row.buy_order_id,
        .ask_order_id = row.sell_order_id,
        .price = row.price,
        .quantity = row.quantity,
        .aggressor_side = row.aggressor_side,
        // N表示集合竞价或无法判定主动方，此时双方都是事实委托；连续竞价
        // B/S成交只扣减被动方。沪市主动单可能直接成交而没有独立委托记录。
        .neutral_trade = row.aggressor_side == tc::AggressorSide::Unknown,
        .kind = md::markets::cn::TransactionKind::Trade,
    };
    const auto mapped = map_trade(view, out);
    return {mapped.ok()
                ? md::mappers::MapResult{md::mappers::MapStatus::Mapped, true}
                : mapped,
            continuity};
  }

  [[nodiscard]] const md::mappers::SequenceStats&
  sequence_stats() const noexcept {
    return sequence_.stats();
  }

 private:
  [[nodiscard]] md::markets::cn::EventContext event_context() const noexcept {
    return {context_.source_id, context_.venue_id, context_.feed_id,
            context_.instrument_id, context_.trading_day};
  }

  [[nodiscard]] md::mappers::MapResult map_order(
      const md::markets::cn::OrderView& view, te::BookOrder& out) const noexcept {
    return context_.market == Market::Shanghai
               ? md::markets::cn::sse::map(event_context(), view, out)
           : context_.market == Market::Shenzhen
               ? md::markets::cn::szse::map(event_context(), view, out)
               : md::mappers::MapResult{md::mappers::MapStatus::Unsupported,
                                        false};
  }

  [[nodiscard]] md::mappers::MapResult map_cancel(
      const md::markets::cn::TransactionView& view,
      te::BookOrder& out) const noexcept {
    return context_.market == Market::Shanghai
               ? md::markets::cn::sse::map(event_context(), view, out)
           : context_.market == Market::Shenzhen
               ? md::markets::cn::szse::map(event_context(), view, out)
               : md::mappers::MapResult{md::mappers::MapStatus::Unsupported,
                                        false};
  }

  [[nodiscard]] md::mappers::MapResult map_phase(
      const md::markets::cn::OrderView& view,
      te::TradingPhaseUpdate& out) const noexcept {
    return context_.market == Market::Shanghai
               ? md::markets::cn::sse::map(event_context(), view, out)
               : md::mappers::MapResult{md::mappers::MapStatus::Unsupported,
                                        false};
  }

  [[nodiscard]] md::mappers::MapResult map_trade(
      const md::markets::cn::TransactionView& view,
      te::BookTrade& out) const noexcept {
    return context_.market == Market::Shanghai
               ? md::markets::cn::sse::map(event_context(), view, out)
           : context_.market == Market::Shenzhen
               ? md::markets::cn::szse::map(event_context(), view, out)
               : md::mappers::MapResult{md::mappers::MapStatus::Unsupported,
                                        false};
  }

  MappingContext context_{};
  md::mappers::SequenceGate sequence_;
};

}  // namespace md::providers::csmar

#pragma once

#include "marketdata/mappers/sequence_gate.hpp"
#include "marketdata/markets/cn/sse/mapper.hpp"
#include "marketdata/markets/cn/szse/mapper.hpp"
#include "trading/events/order_book.hpp"
#include "trading/events/trading_phase.hpp"

#include <cstdint>
#include <string_view>

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

// 这些结构保留希施玛原始字段语义；CSV读取只做十进制定点解析，不解释供应商
// 枚举。沪深表结构不同，使用不同Row防止把Balance、OrderVolume或订单号混用。
struct ShanghaiOrderRow {
  std::uint64_t unix_time_ms{};
  tc::Sequence record_id{};
  tc::PartitionId set_id{};
  tc::OrderId order_id{};
  tc::Price price{};
  tc::Quantity balance{};
  char order_type{};  // A新增、D删除、S产品状态。
  std::string_view order_code{};
};

struct ShanghaiTransactionRow {
  std::uint64_t unix_time_ms{};
  tc::Sequence record_id{};
  tc::PartitionId trade_channel{};
  tc::OrderId buy_order_id{};
  tc::OrderId sell_order_id{};
  tc::Price price{};
  tc::Quantity quantity{};
  char buy_sell_flag{};  // B主动买、S主动卖、N未知。
};

struct ShenzhenOrderRow {
  std::uint64_t unix_time_ms{};
  tc::Sequence record_id{};
  tc::PartitionId set_id{};
  tc::Price price{};
  tc::Quantity quantity{};
  char order_type{};  // 1市价、2限价、U本方最优。
  char order_code{};  // 1买、2卖。
};

struct ShenzhenTransactionRow {
  std::uint64_t unix_time_ms{};
  tc::Sequence record_id{};
  tc::PartitionId set_id{};
  tc::OrderId buy_order_id{};
  tc::OrderId sell_order_id{};
  tc::Price price{};
  tc::Quantity quantity{};
  char trade_type{};  // F成交、4撤单。
};

struct OrderMapOutput {
  tc::EventKind event_kind{tc::EventKind::Unknown};
  te::BookOrder order{};
  te::TradingPhaseUpdate phase{};
};

struct TransactionMapOutput {
  tc::EventKind event_kind{tc::EventKind::Unknown};
  te::BookOrder order{};
  te::BookTrade trade{};
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
      const ShanghaiOrderRow& row, OrderMapOutput& out) noexcept {
    out.event_kind = tc::EventKind::Unknown;
    const auto continuity = sequence_.observe(row.set_id, row.record_id);
    if (!continuity.accepted()) {
      return {{md::mappers::MapStatus::Ignored, false}, continuity};
    }
    if (context_.market != Market::Shanghai || row.unix_time_ms == 0) {
      return {{md::mappers::MapStatus::Invalid, false}, continuity};
    }

    if (row.order_type == 'S') {
      const md::markets::cn::OrderView view{
          .exchange_ts_ns = exchange_time_ns(row.unix_time_ms),
          .event_seq = row.record_id,
          .exchange_seq = row.record_id,
          .partition_id = row.set_id,
          .kind = md::markets::cn::OrderKind::TradingPhase,
          .trading_phase = shanghai_phase(row.order_code),
      };
      const auto mapped =
          md::markets::cn::sse::map(event_context(), view, out.phase);
      out.event_kind = mapped.ok() ? tc::EventKind::TradingPhaseUpdate
                                   : tc::EventKind::Unknown;
      return {mapped, continuity};
    }

    if (row.order_type == 'D') {
      const auto side = shanghai_side(row.order_code);
      const md::markets::cn::TransactionView view{
          .exchange_ts_ns = exchange_time_ns(row.unix_time_ms),
          .event_seq = row.record_id,
          .exchange_seq = row.record_id,
          .partition_id = row.set_id,
          .bid_order_id = side == tc::Side::Buy ? row.order_id : 0,
          .ask_order_id = side == tc::Side::Sell ? row.order_id : 0,
          .quantity = row.balance,
          .kind = md::markets::cn::TransactionKind::Cancel,
      };
      const auto mapped =
          md::markets::cn::sse::map(event_context(), view, out.order);
      out.event_kind =
          mapped.ok() ? tc::EventKind::BookOrder : tc::EventKind::Unknown;
      return {mapped, continuity};
    }

    const md::markets::cn::OrderView view{
        .exchange_ts_ns = exchange_time_ns(row.unix_time_ms),
        .event_seq = row.record_id,
        .exchange_seq = row.record_id,
        .partition_id = row.set_id,
        .order_id = row.order_id,
        .price = row.price,
        // 说明书明确Balance是当前剩余委托量；新增行的TradeVolume是已成交量，
        // 不能重新加入订单簿。
        .quantity = row.balance,
        .side = shanghai_side(row.order_code),
        .kind = row.order_type == 'A'
                    ? md::markets::cn::OrderKind::AddLimit
                    : md::markets::cn::OrderKind::Unknown,
    };
    const auto mapped =
        md::markets::cn::sse::map(event_context(), view, out.order);
    out.event_kind =
        mapped.ok() ? tc::EventKind::BookOrder : tc::EventKind::Unknown;
    return {mapped, continuity};
  }

  [[nodiscard]] md::mappers::MapOutcome map(
      const ShanghaiTransactionRow& row,
      TransactionMapOutput& out) noexcept {
    out.event_kind = tc::EventKind::Unknown;
    const auto continuity =
        sequence_.observe(row.trade_channel, row.record_id);
    if (!continuity.accepted()) {
      return {{md::mappers::MapStatus::Ignored, false}, continuity};
    }
    const bool valid_flag = row.buy_sell_flag == 'B' ||
                            row.buy_sell_flag == 'S' ||
                            row.buy_sell_flag == 'N';
    if (context_.market != Market::Shanghai || row.unix_time_ms == 0 ||
        row.record_id == 0 || row.quantity <= 0 || row.price <= 0 ||
        !valid_flag ||
        (row.buy_order_id == 0 && row.sell_order_id == 0)) {
      return {{md::mappers::MapStatus::Invalid, false}, continuity};
    }

    const auto aggressor = shanghai_aggressor(row.buy_sell_flag);
    const md::markets::cn::TransactionView view{
        .exchange_ts_ns = exchange_time_ns(row.unix_time_ms),
        .event_seq = row.record_id,
        .exchange_seq = row.record_id,
        .partition_id = row.trade_channel,
        .trade_id = row.record_id,
        .bid_order_id = row.buy_order_id,
        .ask_order_id = row.sell_order_id,
        .price = row.price,
        .quantity = row.quantity,
        .aggressor_side = aggressor,
        // N出现在集合竞价或无法判定主动方的撮合，此时买卖双方都是事实委托；
        // 连续竞价B/S只扣减被动方，主动单可能没有独立委托记录。
        .neutral_trade = aggressor == tc::AggressorSide::Unknown,
        .kind = md::markets::cn::TransactionKind::Trade,
    };
    const auto mapped =
        md::markets::cn::sse::map(event_context(), view, out.trade);
    out.event_kind =
        mapped.ok() ? tc::EventKind::BookTrade : tc::EventKind::Unknown;
    return {mapped, continuity};
  }

  [[nodiscard]] md::mappers::MapOutcome map(
      const ShenzhenOrderRow& row, OrderMapOutput& out) noexcept {
    out.event_kind = tc::EventKind::Unknown;
    const auto continuity = sequence_.observe(row.set_id, row.record_id);
    if (!continuity.accepted()) {
      return {{md::mappers::MapStatus::Ignored, false}, continuity};
    }
    if (context_.market != Market::Shenzhen || row.unix_time_ms == 0) {
      return {{md::mappers::MapStatus::Invalid, false}, continuity};
    }

    const md::markets::cn::OrderView view{
        .exchange_ts_ns = exchange_time_ns(row.unix_time_ms),
        .event_seq = row.record_id,
        .exchange_seq = row.record_id,
        .partition_id = row.set_id,
        // 说明书没有独立OrderID；2016-05-09后成交中的Buy/SellOrderID
        // 直接引用混合序列中的委托RecID。
        .order_id = row.record_id,
        .price = row.price,
        .quantity = row.quantity,
        .side = shenzhen_side(row.order_code),
        .kind = shenzhen_order_kind(row.order_type),
    };
    const auto mapped =
        md::markets::cn::szse::map(event_context(), view, out.order);
    out.event_kind =
        mapped.ok() ? tc::EventKind::BookOrder : tc::EventKind::Unknown;
    return {mapped, continuity};
  }

  [[nodiscard]] md::mappers::MapOutcome map(
      const ShenzhenTransactionRow& row,
      TransactionMapOutput& out) noexcept {
    out.event_kind = tc::EventKind::Unknown;
    const auto continuity = sequence_.observe(row.set_id, row.record_id);
    if (!continuity.accepted()) {
      return {{md::mappers::MapStatus::Ignored, false}, continuity};
    }
    if (context_.market != Market::Shenzhen || row.unix_time_ms == 0 ||
        row.record_id == 0 || row.quantity <= 0) {
      return {{md::mappers::MapStatus::Invalid, false}, continuity};
    }

    const bool is_trade = row.trade_type == 'F';
    const bool is_cancel = row.trade_type == '4';
    const bool exactly_one_order =
        (row.buy_order_id == 0) != (row.sell_order_id == 0);
    if ((!is_trade && !is_cancel) ||
        (is_trade && (row.price <= 0 || row.buy_order_id == 0 ||
                      row.sell_order_id == 0)) ||
        (is_cancel && !exactly_one_order)) {
      return {{md::mappers::MapStatus::Invalid, false}, continuity};
    }

    const md::markets::cn::TransactionView view{
        .exchange_ts_ns = exchange_time_ns(row.unix_time_ms),
        .event_seq = row.record_id,
        .exchange_seq = row.record_id,
        .partition_id = row.set_id,
        .trade_id = is_trade ? row.record_id : 0,
        .bid_order_id = row.buy_order_id,
        .ask_order_id = row.sell_order_id,
        .price = row.price,
        .quantity = row.quantity,
        .kind = is_trade ? md::markets::cn::TransactionKind::Trade
                         : md::markets::cn::TransactionKind::Cancel,
    };
    if (is_trade) {
      const auto mapped =
          md::markets::cn::szse::map(event_context(), view, out.trade);
      out.event_kind =
          mapped.ok() ? tc::EventKind::BookTrade : tc::EventKind::Unknown;
      return {mapped, continuity};
    }

    const auto mapped =
        md::markets::cn::szse::map(event_context(), view, out.order);
    out.event_kind =
        mapped.ok() ? tc::EventKind::BookOrder : tc::EventKind::Unknown;
    return {mapped, continuity};
  }

  [[nodiscard]] const md::mappers::SequenceStats&
  sequence_stats() const noexcept {
    return sequence_.stats();
  }

 private:
  // 希施玛UNIX字段是UTC epoch毫秒，而统一行情事件使用交易所本地日内纳秒。
  // 中国市场固定UTC+8且无夏令时；先取模再加偏移，避免远期epoch乘加溢出。
  [[nodiscard]] static constexpr tc::TimestampNs exchange_time_ns(
      std::uint64_t unix_time_ms) noexcept {
    constexpr std::uint64_t kDayMs = 86'400'000ULL;
    constexpr std::uint64_t kShanghaiOffsetMs = 8ULL * 60 * 60 * 1'000;
    return ((unix_time_ms % kDayMs + kShanghaiOffsetMs) % kDayMs) *
           1'000'000ULL;
  }

  [[nodiscard]] static constexpr tc::Side shanghai_side(
      std::string_view code) noexcept {
    return code == "B" ? tc::Side::Buy
         : code == "S" ? tc::Side::Sell
                       : tc::Side::Unknown;
  }

  [[nodiscard]] static constexpr tc::AggressorSide shanghai_aggressor(
      char flag) noexcept {
    return flag == 'B' ? tc::AggressorSide::Buy
         : flag == 'S' ? tc::AggressorSide::Sell
                       : tc::AggressorSide::Unknown;
  }

  [[nodiscard]] static constexpr tc::TradingPhase shanghai_phase(
      std::string_view code) noexcept {
    if (code == "ADD" || code == "START" || code == "PRETR")
      return tc::TradingPhase::Startup;
    if (code == "OCALL" || code == "OOBB" || code == "OPOBB")
      return tc::TradingPhase::OpeningCall;
    if (code == "TRADE") return tc::TradingPhase::Continuous;
    if (code == "BETW" || code == "BREAK")
      return tc::TradingPhase::Break;
    if (code == "SUSP" || code == "HALT" || code == "NOTRD")
      return tc::TradingPhase::Suspended;
    if (code == "CCALL") return tc::TradingPhase::ClosingCall;
    if (code == "CLOSE") return tc::TradingPhase::Closed;
    if (code == "ENDTR") return tc::TradingPhase::EndOfTrading;
    if (code == "POSTR") return tc::TradingPhase::AfterHours;
    if (code == "VOLA" || code == "ICALL" || code == "IOBB" ||
        code == "IPOBB")
      return tc::TradingPhase::VolatilityInterruption;
    if (code == "FCALL") return tc::TradingPhase::FixedPriceCall;
    return tc::TradingPhase::Unknown;
  }

  [[nodiscard]] static constexpr tc::Side shenzhen_side(char code) noexcept {
    return code == '1' ? tc::Side::Buy
         : code == '2' ? tc::Side::Sell
                       : tc::Side::Unknown;
  }

  [[nodiscard]] static constexpr md::markets::cn::OrderKind
  shenzhen_order_kind(char type) noexcept {
    return type == '1' ? md::markets::cn::OrderKind::AddMarket
         : type == '2' ? md::markets::cn::OrderKind::AddLimit
         : type == 'U' ? md::markets::cn::OrderKind::AddOwnBest
                       : md::markets::cn::OrderKind::Unknown;
  }

  [[nodiscard]] md::markets::cn::EventContext event_context() const noexcept {
    return {context_.source_id, context_.venue_id, context_.feed_id,
            context_.instrument_id, context_.trading_day};
  }

  MappingContext context_{};
  md::mappers::SequenceGate sequence_;
};

}  // namespace md::providers::csmar

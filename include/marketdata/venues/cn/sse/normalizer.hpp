#pragma once

#include "marketdata/adapters/map_outcome.hpp"
#include "marketdata/venues/cn/types.hpp"
#include "trading/events/order_book.hpp"
#include "trading/events/status.hpp"

namespace md::venues::cn::sse {

namespace tc = trading::core;
namespace te = trading::events;

[[nodiscard]] inline md::adapters::MapResult normalize(
    const EventContext& context, const OrderView& view,
    te::BookOrder& out) noexcept {
  if (view.kind == OrderKind::Status) {
    return {md::adapters::MapStatus::Ignored, false};
  }
  if (view.order_id == 0 || view.side == tc::Side::Unknown ||
      view.quantity < 0 || view.price < 0) {
    return {md::adapters::MapStatus::Invalid, false};
  }

  te::BookOrder event{};
  fill_header(context, tc::EventKind::BookOrder, view.exchange_ts_ns,
              view.event_seq, view.exchange_seq, event.header);
  event.order_id = view.order_id;
  event.original_order_id = view.original_order_id;
  event.partition_id = view.partition_id;
  event.order_seq = view.exchange_seq;
  event.priority_id = view.order_id;
  event.price = view.price;
  event.quantity = view.quantity;
  event.remaining_qty = view.quantity;
  event.side = view.side;
  event.order_type = tc::OrderType::Limit;
  event.time_in_force = tc::TimeInForce::Day;

  if (view.kind == OrderKind::AddLimit) {
    event.action = tc::OrderAction::Add;
  } else if (view.kind == OrderKind::Delete) {
    event.action = tc::OrderAction::Delete;
  } else {
    return {md::adapters::MapStatus::Unsupported, false};
  }
  out = event;
  return {md::adapters::MapStatus::MappedDiagnostic, false};
}

[[nodiscard]] inline md::adapters::MapResult normalize(
    const EventContext& context, const OrderView& view,
    te::Status& out) noexcept {
  if (view.kind != OrderKind::Status ||
      view.trading_phase == TradingPhase::Unknown) {
    return {md::adapters::MapStatus::Invalid, false};
  }
  te::Status event{};
  fill_header(context, tc::EventKind::Status, view.exchange_ts_ns,
              view.event_seq, view.exchange_seq, event.header);
  event.status_type = tc::StatusType::TradingPhase;
  event.trading_phase = static_cast<std::uint16_t>(view.trading_phase);
  out = event;
  return {md::adapters::MapStatus::Mapped, true};
}

[[nodiscard]] inline md::adapters::MapResult normalize(
    const EventContext& context, const TransactionView& view,
    te::BookTransaction& out) noexcept {
  if (view.trade_id == 0 || view.quantity <= 0 || view.price < 0) {
    return {md::adapters::MapStatus::Invalid, false};
  }

  te::BookTransaction event{};
  fill_header(context, tc::EventKind::BookTransaction,
              view.exchange_ts_ns, view.event_seq, view.exchange_seq,
              event.header);
  event.trade_id = view.trade_id;
  event.transaction_seq = view.trade_id;
  event.partition_id = view.partition_id;
  event.price = view.price;
  event.quantity = view.quantity;
  event.aggressor_side = view.aggressor_side;

  if (view.kind == TransactionKind::Cancel) {
    event.transaction_type = tc::BookTransactionType::Cancel;
    event.canceled_order_id =
        view.ask_order_id != 0 ? view.ask_order_id : view.bid_order_id;
  } else if (view.kind == TransactionKind::Trade) {
    event.transaction_type = tc::BookTransactionType::Trade;
    if (view.exchange_ts_ns < 34'200'000'000'000ULL ||
        view.neutral_trade) {
      event.sell_order_id = view.ask_order_id;
      event.buy_order_id = view.bid_order_id;
      if (view.neutral_trade) {
        event.aggressor_side = tc::AggressorSide::Unknown;
      }
    } else if (view.aggressor_side == tc::AggressorSide::Buy) {
      event.resting_order_id = view.ask_order_id;
    } else if (view.aggressor_side == tc::AggressorSide::Sell) {
      event.resting_order_id = view.bid_order_id;
    } else {
      return {md::adapters::MapStatus::Unsupported, false};
    }
  } else {
    return {md::adapters::MapStatus::Unsupported, false};
  }
  out = event;
  return {md::adapters::MapStatus::MappedDiagnostic, false};
}

}  // namespace md::venues::cn::sse

#pragma once

#include "marketdata/adapters/map_outcome.hpp"
#include "marketdata/venues/cn/types.hpp"
#include "trading/events/order_book.hpp"
#include "trading/events/trading_phase.hpp"

namespace md::venues::cn::sse {

namespace tc = trading::core;
namespace te = trading::events;

[[nodiscard]] inline md::adapters::MapResult normalize(
    const EventContext& context, const OrderView& view,
    te::BookOrder& out) noexcept {
  if (view.kind == OrderKind::TradingPhase) {
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
  event.price_instruction = tc::PriceInstruction::ExplicitLimit;
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
    te::TradingPhaseUpdate& out) noexcept {
  if (view.kind != OrderKind::TradingPhase ||
      view.trading_phase == TradingPhase::Unknown) {
    return {md::adapters::MapStatus::Invalid, false};
  }
  te::TradingPhaseUpdate event{};
  fill_header(context, tc::EventKind::TradingPhaseUpdate, view.exchange_ts_ns,
              view.event_seq, view.exchange_seq, event.header);
  event.trading_phase = view.trading_phase;
  out = event;
  return {md::adapters::MapStatus::Mapped, true};
}

[[nodiscard]] inline md::adapters::MapResult normalize(
    const EventContext& context, const TransactionView& view,
    te::BookOrder& out) noexcept {
  if (view.kind != TransactionKind::Cancel || view.quantity <= 0) {
    return {md::adapters::MapStatus::Invalid, false};
  }
  const auto canceled_order_id =
      view.ask_order_id != 0 ? view.ask_order_id : view.bid_order_id;
  if (canceled_order_id == 0) {
    return {md::adapters::MapStatus::Invalid, false};
  }

  te::BookOrder event{};
  fill_header(context, tc::EventKind::BookOrder, view.exchange_ts_ns,
              view.event_seq, view.exchange_seq, event.header);
  event.order_id = canceled_order_id;
  event.partition_id = view.partition_id;
  event.order_seq = view.exchange_seq;
  event.quantity = view.quantity;
  event.action = tc::OrderAction::Cancel;
  out = event;
  return {md::adapters::MapStatus::MappedDiagnostic, false};
}

[[nodiscard]] inline md::adapters::MapResult normalize(
    const EventContext& context, const TransactionView& view,
    te::BookTrade& out) noexcept {
  if (view.kind != TransactionKind::Trade || view.trade_id == 0 ||
      view.quantity <= 0 || view.price < 0) {
    return {md::adapters::MapStatus::Invalid, false};
  }

  te::BookTrade event{};
  fill_header(context, tc::EventKind::BookTrade,
              view.exchange_ts_ns, view.event_seq, view.exchange_seq,
              event.header);
  event.trade_id = view.trade_id;
  event.trade_seq = view.trade_id;
  event.partition_id = view.partition_id;
  event.price = view.price;
  event.quantity = view.quantity;
  event.aggressor_side = view.aggressor_side;

  if (view.exchange_ts_ns < 34'200'000'000'000ULL || view.neutral_trade) {
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
  out = event;
  return {md::adapters::MapStatus::MappedDiagnostic, false};
}

}  // namespace md::venues::cn::sse

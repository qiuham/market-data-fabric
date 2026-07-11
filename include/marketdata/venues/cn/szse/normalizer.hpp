#pragma once

#include "marketdata/adapters/map_outcome.hpp"
#include "marketdata/venues/cn/types.hpp"
#include "trading/events/order_book.hpp"

namespace md::venues::cn::szse {

namespace tc = trading::core;
namespace te = trading::events;

[[nodiscard]] inline md::adapters::MapResult normalize(
    const EventContext& context, const OrderView& view,
    te::BookOrder& out) noexcept {
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
  event.action = tc::OrderAction::Add;
  event.time_in_force = tc::TimeInForce::Day;

  switch (view.kind) {
    case OrderKind::AddLimit:
      event.order_type = tc::OrderType::Limit;
      break;
    case OrderKind::AddMarket:
      event.order_type = tc::OrderType::Market;
      break;
    case OrderKind::AddOwnBest:
      event.order_type = tc::OrderType::OwnBest;
      break;
    default:
      return {md::adapters::MapStatus::Unsupported, false};
  }
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
  event.sell_order_id = view.ask_order_id;
  event.buy_order_id = view.bid_order_id;
  event.price = view.price;
  event.quantity = view.quantity;
  event.aggressor_side = view.aggressor_side;

  if (view.kind == TransactionKind::Cancel) {
    event.transaction_type = tc::BookTransactionType::Cancel;
    event.canceled_order_id =
        view.ask_order_id != 0 ? view.ask_order_id : view.bid_order_id;
  } else if (view.kind == TransactionKind::Trade) {
    event.transaction_type = tc::BookTransactionType::Trade;
    if (event.aggressor_side == tc::AggressorSide::Unknown &&
        view.bid_order_id != 0 && view.ask_order_id != 0) {
      event.aggressor_side = view.bid_order_id > view.ask_order_id
                                 ? tc::AggressorSide::Buy
                                 : tc::AggressorSide::Sell;
    }
  } else {
    return {md::adapters::MapStatus::Unsupported, false};
  }
  out = event;
  return {md::adapters::MapStatus::Mapped, true};
}

}  // namespace md::venues::cn::szse

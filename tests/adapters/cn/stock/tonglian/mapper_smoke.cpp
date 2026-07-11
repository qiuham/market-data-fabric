#include "marketdata/adapters/tonglian/mapper.hpp"

#include <cassert>

int main() {
  namespace tl = md::adapters::tonglian;
  namespace tc = trading::core;
  namespace te = trading::events;

  tl::MappingContext sz{};
  sz.market = tl::Market::Shenzhen;
  sz.instrument_id = 1;
  sz.trading_day = 20260417;

  te::BookOrder order{};
  auto result = tl::map_order_row(
      sz,
      tl::OrderRow{.time_hhmmssmmm = 93000000,
                    .row_seq = 1,
                    .exchange_order_id = 100,
                    .order_kind = '0',
                    .function_code = 'B',
                    .price = 102400,
                    .quantity = 500,
                    .channel = 2},
      order);
  assert(result.status == tl::MapStatus::Mapped);
  assert(result.lossless);
  assert(order.header.kind == tc::EventKind::BookOrder);
  assert(order.header.trading_day == 20260417);
  assert(order.header.instrument_id == 1);
  assert(order.header.exchange_ts_ns == 34200000000000ULL);
  assert(order.order_id == 100);
  assert(order.partition_id == 2);
  assert(order.side == tc::Side::Buy);
  assert(order.action == tc::OrderAction::Add);
  assert(order.order_type == tc::OrderType::Limit);
  assert(order.price == 102400);
  assert(order.quantity == 500);

  result = tl::map_order_row(
      sz,
      tl::OrderRow{.time_hhmmssmmm = 93000010,
                    .row_seq = 2,
                    .exchange_order_id = 101,
                    .order_kind = '1',
                    .function_code = 'S',
                    .price = 647500,
                    .quantity = 200},
      order);
  assert(result.status == tl::MapStatus::Mapped);
  assert(result.lossless);
  assert(order.order_id == 101);
  assert(order.side == tc::Side::Sell);
  assert(order.order_type == tc::OrderType::Market);
  assert(order.time_in_force == tc::TimeInForce::Day);
  assert(order.price == 647500);

  result = tl::map_order_row(
      sz,
      tl::OrderRow{.time_hhmmssmmm = 93000015,
                    .row_seq = 3,
                    .order_id = 7,
                    .exchange_order_id = 0,
                    .order_kind = '0',
                    .function_code = 'B',
                    .price = 102400,
                    .quantity = 100,
                    .channel = 7},
      order);
  assert(result.status == tl::MapStatus::Invalid);

  result = tl::map_order_row(
      sz,
      tl::OrderRow{.time_hhmmssmmm = 93000020,
                    .row_seq = 4,
                    .exchange_order_id = 102,
                    .order_kind = 'U',
                    .function_code = 'B',
                    .price = 647500,
                    .quantity = 100},
      order);
  assert(result.status == tl::MapStatus::Mapped);
  assert(result.lossless);
  assert(order.order_type == tc::OrderType::OwnBest);
  assert(order.price == 647500);

  tl::TransactionMapOutput transaction_output{};
  auto& transaction = transaction_output.trade;
  result = tl::map_transaction_row(
      sz,
      tl::TransactionRow{.time_hhmmssmmm = 93001000,
                          .row_seq = 4,
                          .transaction_id = 200,
                          .function_code = '0',
                          .bs_flag = 'B',
                          .price = 102400,
                          .quantity = 100,
                          .ask_order_id = 101,
                          .bid_order_id = 100,
                          .channel = 3},
      transaction_output);
  assert(result.status == tl::MapStatus::Mapped);
  assert(result.lossless);
  assert(transaction.header.kind == tc::EventKind::BookTrade);
  assert(transaction_output.event_kind == tc::EventKind::BookTrade);
  assert(transaction.sell_order_id == 101);
  assert(transaction.buy_order_id == 100);
  assert(transaction.partition_id == 3);
  assert(transaction.resting_order_id == 0);
  assert(transaction.aggressor_side == tc::AggressorSide::Buy);

  result = tl::map_transaction_row(
      sz,
      tl::TransactionRow{.time_hhmmssmmm = 93001010,
                          .row_seq = 5,
                          .transaction_id = 202,
                          .function_code = '0',
                          .price = 102400,
                          .quantity = 100,
                          .ask_order_id = 101,
                          .bid_order_id = 105,
                          .channel = 3},
      transaction_output);
  assert(result.status == tl::MapStatus::Mapped);
  assert(transaction.aggressor_side == tc::AggressorSide::Buy);

  result = tl::map_transaction_row(
      sz,
      tl::TransactionRow{.time_hhmmssmmm = 93001020,
                          .row_seq = 6,
                          .transaction_id = 203,
                          .function_code = '0',
                          .price = 102400,
                          .quantity = 100,
                          .ask_order_id = 106,
                          .bid_order_id = 100,
                          .channel = 3},
      transaction_output);
  assert(result.status == tl::MapStatus::Mapped);
  assert(transaction.aggressor_side == tc::AggressorSide::Sell);

  result = tl::map_transaction_row(
      sz,
      tl::TransactionRow{.time_hhmmssmmm = 93002000,
                          .row_seq = 7,
                          .transaction_id = 204,
                          .function_code = 'C',
                          .price = 0,
                          .quantity = 100,
                          .ask_order_id = 0,
                          .bid_order_id = 100},
      transaction_output);
  assert(result.status == tl::MapStatus::Mapped);
  assert(transaction_output.event_kind == tc::EventKind::BookOrder);
  assert(transaction_output.order.action == tc::OrderAction::Cancel);
  assert(transaction_output.order.order_id == 100);

  tl::MappingContext sh{};
  sh.market = tl::Market::Shanghai;
  sh.instrument_id = 2;
  sh.trading_day = 20260417;

  result = tl::map_order_row(
      sh,
      tl::OrderRow{.time_hhmmssmmm = 93000000,
                    .row_seq = 1,
                    .exchange_order_id = 300,
                    .order_kind = 'A',
                    .function_code = 'S',
                    .price = 13600,
                    .quantity = 1000},
      order);
  assert(result.status == tl::MapStatus::MappedDiagnostic);
  assert(!result.lossless);
  assert(order.action == tc::OrderAction::Add);
  assert(order.order_type == tc::OrderType::Limit);

  result = tl::map_order_row(
      sh,
      tl::OrderRow{.time_hhmmssmmm = 93001000,
                    .row_seq = 2,
                    .exchange_order_id = 300,
                    .order_kind = 'D',
                    .function_code = 'S',
                    .price = 13600,
                    .quantity = 1000},
      order);
  assert(result.status == tl::MapStatus::MappedDiagnostic);
  assert(order.action == tc::OrderAction::Delete);

  result = tl::map_order_row(
      sh,
      tl::OrderRow{.time_hhmmssmmm = 91400000,
                    .row_seq = 3,
                    .exchange_order_id = 0,
                    .order_kind = 'S',
                    .function_code = 'I',
                    .price = 0,
                    .quantity = 0},
      order);
  assert(result.status == tl::MapStatus::Ignored);

  trading::events::TradingPhaseUpdate status{};
  result = tl::map_status_row(
      sh,
      tl::OrderRow{.time_hhmmssmmm = 91400000,
                   .row_seq = 3,
                   .order_kind = 'S',
                   .channel = 3,
                   .biz_index = 112,
                   .trading_phase =
                       md::venues::cn::TradingPhase::OpeningCall},
      status);
  assert(result.status == tl::MapStatus::Mapped);
  assert(status.header.exchange_seq == 112);
  assert(status.trading_phase ==
         trading::core::TradingPhase::OpeningCall);

  result = tl::map_transaction_row(
      sh,
      tl::TransactionRow{.time_hhmmssmmm = 93002000,
                          .row_seq = 4,
                          .transaction_id = 400,
                          .function_code = '\0',
                          .bs_flag = 'B',
                          .price = 13600,
                          .quantity = 100,
                          .ask_order_id = 300,
                          .bid_order_id = 999},
      transaction_output);
  assert(result.status == tl::MapStatus::MappedDiagnostic);
  assert(!result.lossless);
  assert(transaction_output.event_kind == tc::EventKind::BookTrade);
  assert(transaction.resting_order_id == 300);
  assert(transaction.buy_order_id == 0);
  assert(transaction.sell_order_id == 0);
  assert(transaction.aggressor_order_id == 0);
  assert(transaction.aggressor_side == tc::AggressorSide::Buy);

  result = tl::map_transaction_row(
      sh,
      tl::TransactionRow{.time_hhmmssmmm = 92501150,
                          .row_seq = 5,
                          .transaction_id = 401,
                          .function_code = '\0',
                          .bs_flag = 'S',
                          .price = 108400,
                          .quantity = 600,
                          .ask_order_id = 301,
                          .bid_order_id = 302},
      transaction_output);
  assert(result.status == tl::MapStatus::MappedDiagnostic);
  assert(!result.lossless);
  assert(transaction_output.event_kind == tc::EventKind::BookTrade);
  assert(transaction.sell_order_id == 301);
  assert(transaction.buy_order_id == 302);
  assert(transaction.resting_order_id == 0);
  assert(transaction.aggressor_side == tc::AggressorSide::Sell);

  result = tl::map_transaction_row(
      sh,
      tl::TransactionRow{.time_hhmmssmmm = 93001150,
                          .row_seq = 6,
                          .transaction_id = 402,
                          .function_code = '\0',
                          .bs_flag = 'N',
                          .price = 108400,
                          .quantity = 600,
                          .ask_order_id = 303,
                          .bid_order_id = 304},
      transaction_output);
  assert(result.status == tl::MapStatus::MappedDiagnostic);
  assert(!result.lossless);
  assert(transaction_output.event_kind == tc::EventKind::BookTrade);
  assert(transaction.sell_order_id == 303);
  assert(transaction.buy_order_id == 304);
  assert(transaction.resting_order_id == 0);
  assert(transaction.aggressor_side == tc::AggressorSide::Unknown);

  return 0;
}

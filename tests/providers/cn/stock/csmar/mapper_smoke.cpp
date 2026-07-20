#include "marketdata/providers/cn/stock/csmar/mapper.hpp"

#include <cassert>

namespace cs = md::providers::csmar;
namespace tc = trading::core;
namespace te = trading::events;

int main() {
  cs::CsmarMapper sh_mapper({.venue_id = 1,
                             .instrument_id = 600730,
                             .trading_day = 20260417,
                             .market = cs::Market::Shanghai});
  assert(sh_mapper.establish_baseline(2, 109));

  cs::OrderMapOutput output{};
  auto result = sh_mapper.map(
      cs::ShanghaiOrderRow{.unix_time_ms = 1'776'388'440'620ULL,
                           .record_id = 110,
                           .set_id = 2,
                           .order_type = 'S',
                           .order_code = "OCALL"},
      output);
  assert(result.publishable());
  assert(output.event_kind == tc::EventKind::TradingPhaseUpdate);
  assert(output.phase.header.exchange_ts_ns == 33'240'620'000'000ULL);

  result = sh_mapper.map(
      cs::ShanghaiOrderRow{.unix_time_ms = 1'776'388'441'000ULL,
                           .record_id = 111,
                           .set_id = 2,
                           .order_type = 'S',
                           .order_code = "SUSP"},
      output);
  assert(result.publishable());
  assert(output.phase.trading_phase == tc::TradingPhase::Suspended);

  result = sh_mapper.map(
      cs::ShanghaiOrderRow{.unix_time_ms = 1'776'388'501'440ULL,
                           .record_id = 74153,
                           .set_id = 2,
                           .order_id = 3674,
                           .price = 91'900,
                           .balance = 1'000,
                           .order_type = 'A',
                           .order_code = "S"},
      output);
  assert(result.publishable());
  assert(result.mapping.status == md::mappers::MapStatus::MappedDiagnostic);
  assert(!result.mapping.lossless);
  assert(output.order.action == tc::OrderAction::Add);
  assert(output.order.order_id == 3674);
  assert(output.order.header.exchange_seq == 74153);

  result = sh_mapper.map(
      cs::ShanghaiOrderRow{.unix_time_ms = 1'776'388'513'350ULL,
                           .record_id = 74320,
                           .set_id = 2,
                           .order_id = 118500,
                           .price = 90'500,
                           .balance = 500,
                           .order_type = 'D',
                           .order_code = "S"},
      output);
  assert(result.publishable());
  assert(output.order.action == tc::OrderAction::Cancel);
  assert(output.order.order_id == 118500);
  assert(output.order.quantity == 500);

  cs::TransactionMapOutput transaction{};
  result = sh_mapper.map(
      cs::ShanghaiTransactionRow{.unix_time_ms = 1'776'389'100'250ULL,
                                 .record_id = 74584,
                                 .trade_channel = 2,
                                 .buy_order_id = 281305,
                                 .sell_order_id = 337667,
                                 .price = 87'500,
                                 .quantity = 100,
                                 .buy_sell_flag = 'N'},
      transaction);
  assert(result.publishable());
  assert(transaction.event_kind == tc::EventKind::BookTrade);
  const auto& trade = transaction.trade;
  assert(trade.buy_order_id == 281305);
  assert(trade.sell_order_id == 337667);
  assert(trade.aggressor_side == tc::AggressorSide::Unknown);

  result = sh_mapper.map(
      cs::ShanghaiTransactionRow{.unix_time_ms = 1'776'389'400'250ULL,
                                 .record_id = 74585,
                                 .trade_channel = 2,
                                 .buy_order_id = 500001,
                                 .sell_order_id = 500002,
                                 .price = 87'500,
                                 .quantity = 100,
                                 .buy_sell_flag = 'B'},
      transaction);
  assert(result.publishable());
  assert(transaction.trade.buy_order_id == 0);
  assert(transaction.trade.sell_order_id == 0);
  assert(transaction.trade.resting_order_id == 500002);
  assert(transaction.trade.aggressor_side == tc::AggressorSide::Buy);

  result = sh_mapper.map(
      cs::ShanghaiTransactionRow{.record_id = 74585, .trade_channel = 2},
      transaction);
  assert(result.continuity.event == md::mappers::ContinuityEvent::Duplicate);
  assert(sh_mapper.sequence_stats().gaps == 0);

  cs::CsmarMapper sz_mapper({.venue_id = 2,
                             .instrument_id = 166,
                             .trading_day = 20260417,
                             .market = cs::Market::Shenzhen});
  assert(sz_mapper.establish_baseline(2012, 206));
  result = sz_mapper.map(
      cs::ShenzhenOrderRow{.unix_time_ms = 1'776'388'500'000ULL,
                           .record_id = 207,
                           .set_id = 2012,
                           .price = 45'000,
                           .quantity = 500,
                           .order_type = '2',
                           .order_code = '1'},
      output);
  assert(result.publishable());
  assert(output.order.order_id == 207);
  assert(output.order.side == tc::Side::Buy);
  assert(output.order.order_type == tc::OrderType::Limit);

  result = sz_mapper.map(
      cs::ShenzhenOrderRow{.unix_time_ms = 1'776'388'500'000ULL,
                           .record_id = 383,
                           .set_id = 2012,
                           .quantity = 100,
                           .order_type = 'U',
                           .order_code = '2'},
      output);
  assert(result.publishable());
  assert(output.order.price_instruction == tc::PriceInstruction::OwnBest);

  result = sz_mapper.map(
      cs::ShenzhenTransactionRow{.unix_time_ms = 1'776'388'500'030ULL,
                                 .record_id = 1915,
                                 .set_id = 2012,
                                 .buy_order_id = 1457,
                                 .quantity = 62'800,
                                 .trade_type = '4'},
      transaction);
  assert(result.publishable());
  assert(transaction.event_kind == tc::EventKind::BookOrder);
  assert(transaction.order.action == tc::OrderAction::Cancel);
  assert(transaction.order.order_id == 1457);

  result = sz_mapper.map(
      cs::ShenzhenTransactionRow{.unix_time_ms = 1'776'388'501'000ULL,
                                 .record_id = 2000,
                                 .set_id = 2012,
                                 .buy_order_id = 1999,
                                 .sell_order_id = 1800,
                                 .price = 45'000,
                                 .quantity = 100,
                                 .trade_type = 'F'},
      transaction);
  assert(result.publishable());
  assert(transaction.event_kind == tc::EventKind::BookTrade);
  assert(transaction.trade.aggressor_side == tc::AggressorSide::Buy);
  return 0;
}

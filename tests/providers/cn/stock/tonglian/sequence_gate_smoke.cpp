#include "marketdata/providers/cn/stock/tonglian/mapper.hpp"

#include <cassert>

namespace tl = md::providers::tonglian;
namespace mm = md::mappers;

int main() {
  mm::SequenceGate gate;
  auto result = gate.observe(3, 100);
  assert(result.event == tl::ContinuityEvent::AwaitingBaseline);
  assert(!result.trusted());

  assert(gate.complete_recovery(3, 99));
  assert(gate.observe(3, 100).accepted());
  assert(gate.observe(3, 101).accepted());

  result = gate.observe(3, 101);
  assert(result.event == tl::ContinuityEvent::Duplicate);
  assert(result.trusted());

  result = gate.observe(3, 104);
  assert(result.event == tl::ContinuityEvent::Gap);
  assert(result.expected == 102);
  assert(!result.trusted());

  result = gate.observe(3, 105);
  assert(result.event == tl::ContinuityEvent::RejectedWhileStale);
  assert(gate.last_accepted() == 101);
  assert(gate.last_received() == 105);

  gate.begin_recovery();
  assert(gate.state() == tl::ContinuityState::Recovering);
  assert(gate.observe_recovery(3, 102).accepted());
  assert(gate.observe_recovery(3, 103).accepted());
  assert(!gate.complete_recovery(3, 105));
  assert(gate.complete_recovery(3, 103));
  assert(gate.observe(3, 104).accepted());

  result = gate.observe(4, 107);
  assert(result.event == tl::ContinuityEvent::WrongPartition);
  assert(!gate.trusted());

  gate.reset();
  assert(!gate.observe(3, 200).accepted());
  assert(gate.complete_recovery(3, 199));
  assert(gate.observe(3, 200).accepted());
  result = gate.observe(3, 199);
  assert(result.event == tl::ContinuityEvent::Regression);

  const auto& stats = gate.stats();
  assert(stats.accepted == 4);
  assert(stats.duplicates == 1);
  assert(stats.gaps == 1);
  assert(stats.regressions == 1);
  assert(stats.wrong_partition == 1);
  assert(stats.rejected_while_stale == 3);

  tl::MappingContext context{};
  context.market = tl::Market::Shanghai;
  tl::TonglianMapper mapper(context);
  assert(mapper.complete_recovery(3, 9));
  tl::OrderMapOutput output{};
  auto mapped = mapper.map(
      tl::OrderRow{.time_hhmmssmmm = 93000000,
                   .exchange_order_id = 42,
                   .order_kind = 'A',
                   .function_code = 'B',
                   .price = 10000,
                   .quantity = 100,
                   .channel = 3,
                   .biz_index = 10},
      output);
  assert(mapped.publishable());
  assert(output.event_kind == trading::core::EventKind::BookOrder);
  assert(output.order.order_id == 42);

  mapped = mapper.map(
      tl::OrderRow{.order_kind = 'S',
                   .channel = 3,
                   .biz_index = 11,
                   .trading_phase =
                       md::markets::cn::TradingPhase::Continuous},
      output);
  assert(mapped.continuity.accepted());
  assert(mapped.publishable());
  assert(output.event_kind == trading::core::EventKind::TradingPhaseUpdate);
  assert(output.phase_update.trading_phase ==
         trading::core::TradingPhase::Continuous);

  mapped = mapper.map(
      tl::OrderRow{.exchange_order_id = 43,
                   .order_kind = 'A',
                   .function_code = 'B',
                   .price = 10000,
                   .quantity = 100,
                   .channel = 3,
                   .biz_index = 13},
      output);
  assert(mapped.continuity.event == tl::ContinuityEvent::Gap);
  assert(!mapped.publishable());
  return 0;
}

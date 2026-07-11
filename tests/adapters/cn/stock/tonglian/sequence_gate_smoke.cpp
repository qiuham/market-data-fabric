#include "marketdata/adapters/tonglian/mapper.hpp"

#include <cassert>

namespace tl = md::adapters::tonglian;

int main() {
  tl::TonglianSequenceGate gate;
  auto result = gate.observe(3, 100);
  assert(result.event == tl::ContinuityEvent::AwaitingBaseline);
  assert(!result.trusted());

  gate.complete_recovery(3, 99);
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
  gate.complete_recovery(3, 105);
  assert(gate.observe(3, 106).accepted());

  result = gate.observe(4, 107);
  assert(result.event == tl::ContinuityEvent::WrongPartition);
  assert(!gate.book_trusted());

  gate.reset();
  assert(!gate.observe(3, 200).accepted());
  gate.complete_recovery(3, 199);
  assert(gate.observe(3, 200).accepted());
  result = gate.observe(3, 199);
  assert(result.event == tl::ContinuityEvent::Regression);

  const auto& stats = gate.stats();
  assert(stats.accepted == 4);
  assert(stats.duplicates == 1);
  assert(stats.gaps == 1);
  assert(stats.regressions == 1);
  assert(stats.wrong_channel == 1);
  assert(stats.rejected_while_stale == 3);

  tl::MappingContext context{};
  context.market = tl::Market::Shanghai;
  tl::TonglianMapper mapper(context);
  mapper.complete_recovery(3, 9);
  trading::events::BookOrder order{};
  auto mapped = mapper.map(
      tl::OrderRow{.time_hhmmssmmm = 93000000,
                   .exchange_order_id = 42,
                   .order_kind = 'A',
                   .function_code = 'B',
                   .price = 10000,
                   .quantity = 100,
                   .channel = 3,
                   .biz_index = 10},
      order);
  assert(mapped.publishable());
  assert(order.order_id == 42);

  mapped = mapper.map(
      tl::OrderRow{.order_kind = 'S', .channel = 3, .biz_index = 11}, order);
  assert(mapped.continuity.accepted());
  assert(mapped.mapping.status == tl::MapStatus::Ignored);
  assert(!mapped.publishable());

  mapped = mapper.map(
      tl::OrderRow{.exchange_order_id = 43,
                   .order_kind = 'A',
                   .function_code = 'B',
                   .price = 10000,
                   .quantity = 100,
                   .channel = 3,
                   .biz_index = 13},
      order);
  assert(mapped.continuity.event == tl::ContinuityEvent::Gap);
  assert(!mapped.publishable());
  return 0;
}

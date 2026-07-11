#include "marketdata/replay/event_journal.hpp"
#include "trading/events/l3.hpp"

#include <cassert>
#include <filesystem>

int main() {
  const auto path =
      std::filesystem::temp_directory_path() / "marketdata_event_journal_test";
  {
    md::replay::EventJournalWriter writer(path, 1001, 20260417);
    trading::events::BookOrder order{};
    order.header.exchange_seq = 10;
    order.header.event_seq = 20;
    order.order_id = 42;
    order.price = 10000;
    order.quantity = 100;
    assert(writer.append(order, 3));

    trading::events::BookTransaction transaction{};
    transaction.header.exchange_seq = 11;
    transaction.trade_id = 77;
    transaction.buy_order_id = 42;
    transaction.price = 10000;
    transaction.quantity = 100;
    assert(writer.append(transaction, 3));
    writer.flush();
  }

  md::replay::EventJournalReader reader(path);
  assert(reader.valid());
  assert(reader.header().source_id == 1001);
  assert(reader.header().trading_day == 20260417);

  md::replay::EventJournalRecord record{};
  assert(reader.next(record));
  assert(record.header.partition_id == 3);
  trading::events::BookOrder order{};
  assert(record.decode(order));
  assert(order.order_id == 42);

  assert(reader.next(record));
  trading::events::BookTransaction transaction{};
  assert(record.decode(transaction));
  assert(transaction.trade_id == 77);
  assert(!reader.next(record));

  std::filesystem::remove(path);
  return 0;
}

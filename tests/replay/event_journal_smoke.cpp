#include "marketdata/replay/event_journal.hpp"
#include "trading/events/order_book.hpp"

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

    md::replay::AsyncEventJournalQueue<2> queue;
    trading::events::BookOrder queued = order;
    queued.header.exchange_seq = 12;
    queued.order_id = 43;
    assert(queue.try_enqueue(queued, 3) ==
           md::replay::JournalEnqueueStatus::Queued);
    assert(queue.drain_one(writer));
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
  assert(reader.next(record));
  assert(record.decode(order));
  assert(order.order_id == 43);
  assert(!reader.next(record));
  assert(reader.last_status() == md::replay::EventJournalReadStatus::EndOfFile);

  const auto truncated = path.string() + ".truncated";
  std::filesystem::copy_file(path, truncated,
                             std::filesystem::copy_options::overwrite_existing);
  std::filesystem::resize_file(truncated,
                               std::filesystem::file_size(truncated) - 5);
  md::replay::EventJournalReader truncated_reader(truncated);
  assert(truncated_reader.next(record));
  assert(truncated_reader.next(record));
  assert(!truncated_reader.next(record));
  assert(truncated_reader.last_status() ==
         md::replay::EventJournalReadStatus::TruncatedTail);

  std::filesystem::remove(path);
  std::filesystem::remove(truncated);
  return 0;
}

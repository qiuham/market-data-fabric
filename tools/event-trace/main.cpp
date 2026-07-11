#include "marketdata/replay/event_journal.hpp"
#include "orderbook/rebuild/mbo_rebuilder.hpp"
#include "trading/events/l3.hpp"

#include <charconv>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace ob = orderbook::rebuild;
namespace tc = trading::core;
namespace te = trading::events;

[[nodiscard]] std::uint64_t parse_id(std::string_view text) {
  std::uint64_t value{};
  const auto parsed =
      std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} ? value : 0;
}

[[nodiscard]] bool references(const te::BookTransaction& event,
                              tc::OrderId order_id) noexcept {
  return order_id != 0 &&
         (event.buy_order_id == order_id || event.sell_order_id == order_id ||
          event.canceled_order_id == order_id ||
          event.resting_order_id == order_id ||
          event.aggressor_order_id == order_id);
}

int main(int argc, char** argv) {
  if (argc != 4 || (std::string_view(argv[2]) != "--order" &&
                    std::string_view(argv[2]) != "--trade")) {
    std::cerr << "用法: " << argv[0]
              << " JOURNAL (--order ORDER_ID | --trade TRADE_ID)\n";
    return 2;
  }
  const auto id = parse_id(argv[3]);
  if (id == 0) {
    std::cerr << "订单或成交编号必须是正整数\n";
    return 2;
  }

  const bool trace_order = std::string_view(argv[2]) == "--order";
  md::replay::EventJournalReader reader(argv[1]);
  if (!reader.valid()) {
    std::cerr << "无法读取标准事件日志: " << argv[1] << '\n';
    return 1;
  }

  ob::MboRebuilder book;
  md::replay::EventJournalRecord record{};
  std::uint64_t matches{};
  while (reader.next(record)) {
    te::BookOrder order{};
    if (record.decode(order)) {
      (void)book.apply(order);
      if (trace_order &&
          (order.order_id == id || order.original_order_id == id)) {
        const auto* active = book.book().find_order(id);
        std::cout << "订单事件\t序号=" << order.header.exchange_seq
                  << "\t动作=" << static_cast<int>(order.action)
                  << "\t价格=" << order.price << "\t数量=" << order.quantity
                  << "\t剩余="
                  << (active == nullptr ? 0 : active->remaining_qty) << '\n';
        ++matches;
      }
      continue;
    }

    te::BookTransaction transaction{};
    if (!record.decode(transaction)) {
      continue;
    }
    (void)book.apply(transaction);
    if ((trace_order && references(transaction, id)) ||
        (!trace_order && transaction.trade_id == id)) {
      std::cout << "成交事件\t序号=" << transaction.header.exchange_seq
                << "\t成交编号=" << transaction.trade_id
                << "\t买单=" << transaction.buy_order_id
                << "\t卖单=" << transaction.sell_order_id
                << "\t价格=" << transaction.price
                << "\t数量=" << transaction.quantity;
      if (trace_order) {
        const auto* active = book.book().find_order(id);
        std::cout << "\t剩余="
                  << (active == nullptr ? 0 : active->remaining_qty);
      }
      std::cout << '\n';
      ++matches;
    }
  }

  if (matches == 0) {
    std::cerr << "标准事件日志中未找到目标编号\n";
    return 3;
  }
  return 0;
}

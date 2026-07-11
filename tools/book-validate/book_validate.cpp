#include "marketdata/adapters/tonglian/mapper.hpp"
#include "marketdata/replay/event_journal.hpp"
#include "marketdata/venues/trading_phase_tracker.hpp"
#include "orderbook/rebuild/mbo_rebuilder.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <variant>
#include <unordered_set>
#include <vector>

namespace {

namespace fs = std::filesystem;
namespace ob = orderbook::rebuild;
namespace tc = trading::core;
namespace te = trading::events;
namespace tl = md::adapters::tonglian;

struct Options {
  fs::path root;
  std::vector<std::string> symbols;
  tl::Market market{tl::Market::Unknown};
  int limit{8};
  int mismatch_limit{3};
  fs::path journal_dir{};
  bool summary{};
};

[[nodiscard]] std::string trim(std::string_view value) {
  const auto first = value.find_first_not_of(" \t\r\n\0");
  if (first == std::string_view::npos) {
    return {};
  }
  const auto last = value.find_last_not_of(" \t\r\n\0");
  return std::string(value.substr(first, last - first + 1));
}

[[nodiscard]] std::vector<std::string> split_csv(const std::string& line) {
  std::vector<std::string> out;
  std::string field;
  bool quoted{};
  for (std::size_t index = 0; index < line.size(); ++index) {
    const auto ch = line[index];
    if (ch == '"') {
      if (quoted && index + 1 < line.size() && line[index + 1] == '"') {
        field.push_back('"');
        ++index;
      } else {
        quoted = !quoted;
      }
    } else if (ch == ',' && !quoted) {
      out.push_back(trim(field));
      field.clear();
    } else {
      field.push_back(ch);
    }
  }
  out.push_back(trim(field));
  return out;
}

template <typename Int>
[[nodiscard]] Int to_int(std::string_view value) {
  const auto text = trim(value);
  if (text.empty()) {
    return {};
  }
  Int out{};
  const auto result =
      std::from_chars(text.data(), text.data() + text.size(), out);
  return result.ec == std::errc{} ? out : Int{};
}

[[nodiscard]] tc::Price to_price(std::string_view value) {
  const auto text = trim(value);
  if (text.empty()) {
    return 0;
  }
  bool negative{};
  std::size_t index{};
  if (text.front() == '-') {
    negative = true;
    ++index;
  }
  tc::Price integer{};
  while (index < text.size() && text[index] != '.') {
    if (text[index] < '0' || text[index] > '9') {
      return 0;
    }
    integer = integer * 10 + (text[index++] - '0');
  }
  tc::Price fraction{};
  tc::Price scale{1000};
  if (index < text.size() && text[index] == '.') {
    ++index;
    while (index < text.size() && scale > 0) {
      if (text[index] < '0' || text[index] > '9') {
        break;
      }
      fraction += static_cast<tc::Price>(text[index++] - '0') * scale;
      scale /= 10;
    }
  }
  const auto price = integer * 10000 + fraction;
  return negative ? -price : price;
}

[[nodiscard]] std::uint32_t to_time(std::string_view value) {
  std::string digits;
  digits.reserve(9);
  for (const auto ch : value) {
    if (ch >= '0' && ch <= '9') {
      digits.push_back(ch);
    }
  }
  return to_int<std::uint32_t>(digits);
}

[[nodiscard]] char enum_char(std::string_view value) {
  const auto text = trim(value);
  if (text.size() == 1) {
    return text.front();
  }
  const auto code = to_int<int>(text);
  return code > 0 && code <= 255 ? static_cast<char>(code) : '\0';
}

[[nodiscard]] char side_char(std::string_view value) {
  switch (enum_char(value)) {
    case '1':
    case 'B':
      return 'B';
    case '2':
    case 'S':
      return 'S';
    default:
      return '\0';
  }
}

[[nodiscard]] char sz_order_kind(std::string_view value) {
  switch (enum_char(value)) {
    case '1':
      return '1';
    case '2':
      return '0';
    case 'U':
      return 'U';
    default:
      return '\0';
  }
}

[[nodiscard]] char sz_exec_type(std::string_view value) {
  switch (enum_char(value)) {
    case 'F':
    case '0':
      return '0';
    case '4':
    case 'C':
      return 'C';
    default:
      return '\0';
  }
}

[[nodiscard]] std::string shell_quote(std::string_view value) {
  std::string out{"'"};
  for (const auto ch : value) {
    if (ch == '\'') {
      out += "'\\''";
    } else {
      out.push_back(ch);
    }
  }
  out.push_back('\'');
  return out;
}

[[nodiscard]] std::string parquet_python() {
  if (const auto* value = std::getenv("CN_BOOK_CHECK_PYTHON"); value != nullptr) {
    return value;
  }
  return "python3";
}

[[nodiscard]] fs::path parquet_helper() {
  if (const auto* value = std::getenv("CN_BOOK_CHECK_PARQUET_HELPER");
      value != nullptr) {
    return value;
  }
  const auto local = fs::current_path() / "tools/book-validate/parquet_rows.py";
  return fs::exists(local) ? local : fs::path{"tools/book-validate/parquet_rows.py"};
}

class ParquetRows {
 public:
  ParquetRows(const fs::path& path, std::optional<std::string_view> symbol) {
    auto command = shell_quote(parquet_python()) + " " +
                   shell_quote(parquet_helper().string()) + " --file " +
                   shell_quote(path.string());
    if (symbol.has_value()) {
      command += " --symbol " + shell_quote(*symbol);
    } else {
      command += " --list-symbols";
    }
    pipe_ = popen(command.c_str(), "r");
  }

  ParquetRows(const ParquetRows&) = delete;
  ParquetRows& operator=(const ParquetRows&) = delete;

  ~ParquetRows() {
    if (pipe_ != nullptr) {
      (void)pclose(pipe_);
    }
  }

  [[nodiscard]] bool ok() const noexcept { return pipe_ != nullptr; }

  [[nodiscard]] bool getline(std::string& line) {
    if (pipe_ == nullptr ||
        std::fgets(buffer_.data(), static_cast<int>(buffer_.size()), pipe_) ==
            nullptr) {
      return false;
    }
    line.assign(buffer_.data());
    return true;
  }

 private:
  FILE* pipe_{};
  std::array<char, 1 << 20> buffer_{};
};

class Columns {
 public:
  explicit Columns(const std::string& header) {
    const auto names = split_csv(header);
    for (std::size_t index = 0; index < names.size(); ++index) {
      indices_.emplace(names[index], index);
    }
  }

  [[nodiscard]] bool require(
      std::initializer_list<std::string_view> names) const {
    return std::all_of(names.begin(), names.end(), [&](const auto name) {
      return indices_.contains(std::string(name));
    });
  }

  [[nodiscard]] std::string_view get(const std::vector<std::string>& row,
                                     std::string_view name) const {
    const auto iter = indices_.find(std::string(name));
    if (iter == indices_.end() || iter->second >= row.size()) {
      return {};
    }
    return row[iter->second];
  }

 private:
  std::unordered_map<std::string, std::size_t> indices_;
};

[[nodiscard]] std::string base_symbol(std::string_view symbol) {
  const auto dot = symbol.find('.');
  return std::string(dot == std::string_view::npos ? symbol
                                                   : symbol.substr(0, dot));
}

[[nodiscard]] tl::Market symbol_market(std::string_view symbol) {
  if (symbol.ends_with(".SH")) {
    return tl::Market::Shanghai;
  }
  if (symbol.ends_with(".SZ")) {
    return tl::Market::Shenzhen;
  }
  return tl::Market::Unknown;
}

[[nodiscard]] const char* market_name(tl::Market market) {
  return market == tl::Market::Shanghai ? "沪"
       : market == tl::Market::Shenzhen ? "深"
                                        : "未知";
}

[[nodiscard]] fs::path market_dir(const fs::path& root, tl::Market market) {
  return root / (market == tl::Market::Shanghai ? "sh" : "sz");
}

[[nodiscard]] fs::path snapshot_path(const fs::path& root, tl::Market market) {
  return market_dir(root, market) / "market_snapshot.parquet";
}

struct SequenceHealth {
  tc::PartitionId channel{};
  tc::Sequence first{};
  tc::Sequence last{};
  std::uint64_t duplicates{};
  std::uint64_t regressions{};
  std::uint64_t channel_changes{};

  void observe(tc::PartitionId next_channel, tc::Sequence sequence) {
    if (channel == 0) {
      channel = next_channel;
    } else if (channel != next_channel) {
      ++channel_changes;
    }
    if (first == 0) {
      first = sequence;
    }
    if (last != 0) {
      if (sequence == last) {
        ++duplicates;
      } else if (sequence < last) {
        ++regressions;
      }
    }
    last = sequence;
  }
};

enum class Payload { Order, Transaction, Status };

struct Event {
  tc::PartitionId channel{};
  tc::Sequence sequence{};
  std::uint32_t time{};
  std::uint8_t priority{};
  Payload payload{Payload::Order};
  std::variant<te::BookOrder, te::BookTransaction, te::Status> data{};

  [[nodiscard]] bool is_trade() const noexcept {
    const auto* transaction = std::get_if<te::BookTransaction>(&data);
    return transaction != nullptr &&
           transaction->transaction_type == tc::BookTransactionType::Trade;
  }
};

[[nodiscard]] md::venues::cn::TradingPhase sh_phase(
    std::string_view value) noexcept {
  using Phase = md::venues::cn::TradingPhase;
  const auto phase = trim(value);
  return phase == "OCALL" ? Phase::OpeningCall
       : phase == "TRADE" ? Phase::Continuous
       : phase == "CCALL" ? Phase::ClosingCall
       : phase == "CLOSE" ? Phase::Closed
       : phase == "ENDTR" ? Phase::EndOfTrading
                          : Phase::Unknown;
}

struct LoadHealth {
  SequenceHealth sequence;
  std::uint64_t status_rows{};
  std::uint64_t invalid_rows{};
  std::uint64_t unsupported_rows{};
};

[[nodiscard]] tl::MappingContext mapping_context(const fs::path& root,
                                                 tl::Market market) {
  tl::MappingContext context{};
  context.market = market;
  context.trading_day = to_int<tc::TradingDay>(root.filename().string());
  return context;
}

[[nodiscard]] bool mapped(const tl::MapResult& result,
                          LoadHealth& health) {
  if (result.ok()) {
    return true;
  }
  if (result.status == tl::MapStatus::Unsupported) {
    ++health.unsupported_rows;
  } else if (result.status != tl::MapStatus::Ignored) {
    ++health.invalid_rows;
  }
  return false;
}

[[nodiscard]] std::vector<Event> load_sh_events(const Options& options,
                                                std::string_view symbol,
                                                LoadHealth& health) {
  const auto path = market_dir(options.root, tl::Market::Shanghai) /
                    "tick_by_tick.parquet";
  ParquetRows input(path, base_symbol(symbol));
  std::string line;
  if (!input.ok() || !input.getline(line)) {
    throw std::runtime_error("cannot read " + path.string());
  }
  const Columns columns(line);
  if (!columns.require({"BizIndex", "Channel", "TickTime", "Type",
                        "BuyOrderNO", "SellOrderNO", "Price", "Qty",
                        "TickBSFlag", "SeqNo"})) {
    throw std::runtime_error("unexpected Shanghai tick schema");
  }

  const auto context = mapping_context(options.root, tl::Market::Shanghai);
  std::vector<Event> events;
  while (input.getline(line)) {
    const auto row = split_csv(line);
    const auto sequence =
        to_int<tc::Sequence>(columns.get(row, "BizIndex"));
    const auto channel =
        to_int<tc::PartitionId>(columns.get(row, "Channel"));
    health.sequence.observe(channel, sequence);
    const auto type = enum_char(columns.get(row, "Type"));
    if (type == 'S') {
      ++health.status_rows;
      tl::OrderRow source{
          .time_hhmmssmmm = to_time(columns.get(row, "TickTime")),
          .row_seq = to_int<tc::Sequence>(columns.get(row, "SeqNo")),
          .order_kind = type,
          .channel = channel,
          .biz_index = sequence,
          .trading_phase = sh_phase(columns.get(row, "TickBSFlag")),
      };
      te::Status status{};
      const auto result = tl::map_status_row(context, source, status);
      if (mapped(result, health)) {
        events.push_back(Event{.channel = channel,
                               .sequence = sequence,
                               .time = source.time_hhmmssmmm,
                               .priority = 0,
                               .payload = Payload::Status,
                               .data = status});
      }
      continue;
    }
    const auto time = to_time(columns.get(row, "TickTime"));
    const auto side = side_char(columns.get(row, "TickBSFlag"));
    const auto buy_order_id =
        to_int<tc::OrderId>(columns.get(row, "BuyOrderNO"));
    const auto sell_order_id =
        to_int<tc::OrderId>(columns.get(row, "SellOrderNO"));

    if (type == 'A' || type == 'D') {
      const auto order_id = side == 'B' ? buy_order_id : sell_order_id;
      tl::OrderRow source{
          .time_hhmmssmmm = time,
          .row_seq = to_int<tc::Sequence>(columns.get(row, "SeqNo")),
          .order_id = order_id,
          .exchange_order_id = order_id,
          .order_kind = type,
          .function_code = side,
          .price = to_price(columns.get(row, "Price")),
          .quantity = to_int<tc::Quantity>(columns.get(row, "Qty")),
          .channel = channel,
          .biz_index = sequence,
      };
      te::BookOrder order{};
      const auto result = tl::map_order_row(context, source, order);
      if (mapped(result, health)) {
        events.push_back(Event{.channel = channel,
                               .sequence = sequence,
                               .time = time,
                               .priority = 0,
                               .payload = Payload::Order,
                               .data = order});
      }
      continue;
    }

    if (type == 'T') {
      tl::TransactionRow source{
          .time_hhmmssmmm = time,
          .row_seq = to_int<tc::Sequence>(columns.get(row, "SeqNo")),
          .transaction_id = sequence,
          .function_code = '\0',
          .order_kind = type,
          .bs_flag = enum_char(columns.get(row, "TickBSFlag")),
          .price = to_price(columns.get(row, "Price")),
          .quantity = to_int<tc::Quantity>(columns.get(row, "Qty")),
          .ask_order_id = sell_order_id,
          .bid_order_id = buy_order_id,
          .channel = channel,
          .biz_index = sequence,
      };
      te::BookTransaction transaction{};
      const auto result =
          tl::map_transaction_row(context, source, transaction);
      if (mapped(result, health)) {
        events.push_back(Event{.channel = channel,
                               .sequence = sequence,
                               .time = time,
                               .priority = 1,
                               .payload = Payload::Transaction,
                               .data = transaction});
      }
      continue;
    }
    ++health.unsupported_rows;
  }
  return events;
}

void append_sz_orders(const Options& options, std::string_view symbol,
                      std::vector<Event>& events, LoadHealth& health) {
  const auto path = market_dir(options.root, tl::Market::Shenzhen) /
                    "orders.parquet";
  ParquetRows input(path, base_symbol(symbol));
  std::string line;
  if (!input.ok() || !input.getline(line)) {
    throw std::runtime_error("cannot read " + path.string());
  }
  const Columns columns(line);
  if (!columns.require({"ChannelNo", "ApplSeqNum", "Price", "OrderQty",
                        "Side", "TransactTime", "OrdType", "SeqNo"})) {
    throw std::runtime_error("unexpected Shenzhen order schema");
  }
  const auto context = mapping_context(options.root, tl::Market::Shenzhen);
  while (input.getline(line)) {
    const auto row = split_csv(line);
    const auto channel =
        to_int<tc::PartitionId>(columns.get(row, "ChannelNo"));
    const auto sequence =
        to_int<tc::Sequence>(columns.get(row, "ApplSeqNum"));
    const auto time = to_time(columns.get(row, "TransactTime"));
    tl::OrderRow source{
        .time_hhmmssmmm = time,
        .row_seq = to_int<tc::Sequence>(columns.get(row, "SeqNo")),
        .order_id = sequence,
        .exchange_order_id = sequence,
        .order_kind = sz_order_kind(columns.get(row, "OrdType")),
        .function_code = side_char(columns.get(row, "Side")),
        .price = to_price(columns.get(row, "Price")),
        .quantity = to_int<tc::Quantity>(columns.get(row, "OrderQty")),
        .channel = channel,
        .biz_index = sequence,
    };
    te::BookOrder order{};
    const auto result = tl::map_order_row(context, source, order);
    if (mapped(result, health)) {
      events.push_back(Event{.channel = channel,
                             .sequence = sequence,
                             .time = time,
                             .priority = 0,
                             .payload = Payload::Order,
                             .data = order});
    }
  }
}

void append_sz_transactions(const Options& options, std::string_view symbol,
                            std::vector<Event>& events, LoadHealth& health) {
  const auto path = market_dir(options.root, tl::Market::Shenzhen) /
                    "transactions.parquet";
  ParquetRows input(path, base_symbol(symbol));
  std::string line;
  if (!input.ok() || !input.getline(line)) {
    throw std::runtime_error("cannot read " + path.string());
  }
  const Columns columns(line);
  if (!columns.require({"ChannelNo", "ApplSeqNum", "BidApplSeqNum",
                        "OfferApplSeqNum", "LastPx", "LastQty", "ExecType",
                        "TransactTime", "SeqNo"})) {
    throw std::runtime_error("unexpected Shenzhen transaction schema");
  }
  const auto context = mapping_context(options.root, tl::Market::Shenzhen);
  while (input.getline(line)) {
    const auto row = split_csv(line);
    const auto channel =
        to_int<tc::PartitionId>(columns.get(row, "ChannelNo"));
    const auto sequence =
        to_int<tc::Sequence>(columns.get(row, "ApplSeqNum"));
    const auto time = to_time(columns.get(row, "TransactTime"));
    tl::TransactionRow source{
        .time_hhmmssmmm = time,
        .row_seq = to_int<tc::Sequence>(columns.get(row, "SeqNo")),
        .transaction_id = sequence,
        .function_code = sz_exec_type(columns.get(row, "ExecType")),
        .price = to_price(columns.get(row, "LastPx")),
        .quantity = to_int<tc::Quantity>(columns.get(row, "LastQty")),
        .ask_order_id =
            to_int<tc::OrderId>(columns.get(row, "OfferApplSeqNum")),
        .bid_order_id =
            to_int<tc::OrderId>(columns.get(row, "BidApplSeqNum")),
        .channel = channel,
        .biz_index = sequence,
    };
    te::BookTransaction transaction{};
    const auto result = tl::map_transaction_row(context, source, transaction);
    if (mapped(result, health)) {
      events.push_back(Event{.channel = channel,
                             .sequence = sequence,
                             .time = time,
                             .priority = 1,
                             .payload = Payload::Transaction,
                             .data = transaction});
    }
  }
}

[[nodiscard]] std::vector<Event> load_events(const Options& options,
                                             std::string_view symbol,
                                             tl::Market market,
                                             LoadHealth& health) {
  std::vector<Event> events;
  if (market == tl::Market::Shanghai) {
    events = load_sh_events(options, symbol, health);
  } else {
    append_sz_orders(options, symbol, events, health);
    append_sz_transactions(options, symbol, events, health);
    std::sort(events.begin(), events.end(), [](const Event& lhs, const Event& rhs) {
      if (lhs.channel != rhs.channel) {
        return lhs.channel < rhs.channel;
      }
      if (lhs.sequence != rhs.sequence) {
        return lhs.sequence < rhs.sequence;
      }
      return lhs.priority < rhs.priority;
    });
    for (const auto& event : events) {
      health.sequence.observe(event.channel, event.sequence);
    }
  }
  return events;
}

struct LevelKey {
  tc::Price price{};
  tc::Quantity quantity{};
  friend bool operator==(const LevelKey&, const LevelKey&) = default;
};

struct BookKey {
  tc::Quantity total_bid_qty{};
  tc::Quantity total_ask_qty{};
  std::array<LevelKey, 10> bids{};
  std::array<LevelKey, 10> asks{};
  std::uint8_t bid_count{};
  std::uint8_t ask_count{};
  friend bool operator==(const BookKey&, const BookKey&) = default;
};

struct TradeAnchor {
  std::uint64_t count{};
  tc::Quantity volume{};
  tc::Amount amount{};
  friend bool operator==(const TradeAnchor&, const TradeAnchor&) = default;
};

struct TradeAnchorHash {
  [[nodiscard]] std::size_t operator()(const TradeAnchor& value) const noexcept {
    auto hash = static_cast<std::size_t>(value.count);
    hash ^= static_cast<std::size_t>(value.volume) + 0x9e3779b97f4a7c15ULL +
            (hash << 6U) + (hash >> 2U);
    hash ^= static_cast<std::size_t>(value.amount) + 0x9e3779b97f4a7c15ULL +
            (hash << 6U) + (hash >> 2U);
    return hash;
  }
};

struct Snapshot {
  std::uint32_t time{};
  TradeAnchor anchor{};
  BookKey expected{};
  bool anchor_seen{};
  std::uint32_t match_count{};
  tc::Sequence first_match_sequence{};
  tc::Sequence last_match_sequence{};
};

[[nodiscard]] bool continuous_snapshot(tl::Market market, std::uint32_t time,
                                       std::string_view status) {
  const auto phase = trim(status);
  if (market == tl::Market::Shanghai) {
    if (!phase.empty()) {
      return phase == "TRADE";
    }
    return time >= 93000000 && time < 145700000;
  }
  return time >= 93000000 && time < 145700000;
}

[[nodiscard]] std::vector<Snapshot> load_snapshots(
    const Options& options, std::string_view symbol, tl::Market market,
    std::uint64_t& skipped) {
  const auto path = snapshot_path(options.root, market);
  ParquetRows input(path, base_symbol(symbol));
  std::string line;
  if (!input.ok() || !input.getline(line)) {
    throw std::runtime_error("cannot read " + path.string());
  }
  const Columns columns(line);
  const bool sh = market == tl::Market::Shanghai;
  const auto phase_name = sh ? "InstruStatus" : "TradingPhaseCode";
  const auto count_name = sh ? "TradNumber" : "TurnNum";
  const auto volume_name = sh ? "TradVolume" : "Volume";
  constexpr auto amount_name = "Turnover";
  const auto bid_total_name = sh ? "TotalBidVol" : "TotalBidQty";
  const auto ask_total_name = sh ? "TotalAskVol" : "TotalOfferQty";
  if (!columns.require({"UpdateTime", phase_name, count_name, volume_name,
                        amount_name,
                        bid_total_name, ask_total_name, "BidPrice1",
                        "BidVolume1", "AskPrice1", "AskVolume1"})) {
    throw std::runtime_error("unexpected snapshot schema");
  }

  std::vector<Snapshot> snapshots;
  while (input.getline(line)) {
    const auto row = split_csv(line);
    const auto time = to_time(columns.get(row, "UpdateTime"));
    if (!continuous_snapshot(market, time, columns.get(row, phase_name))) {
      ++skipped;
      continue;
    }
    Snapshot snapshot{
        .time = time,
        .anchor = TradeAnchor{
            .count = to_int<std::uint64_t>(columns.get(row, count_name)),
            .volume = to_int<tc::Quantity>(columns.get(row, volume_name)),
            .amount = to_price(columns.get(row, amount_name)),
        },
    };
    snapshot.expected.total_bid_qty =
        to_int<tc::Quantity>(columns.get(row, bid_total_name));
    snapshot.expected.total_ask_qty =
        to_int<tc::Quantity>(columns.get(row, ask_total_name));
    for (std::size_t level = 0; level < 10; ++level) {
      const auto number = std::to_string(level + 1);
      const auto bid_price =
          to_price(columns.get(row, "BidPrice" + number));
      const auto bid_qty =
          to_int<tc::Quantity>(columns.get(row, "BidVolume" + number));
      const auto ask_price =
          to_price(columns.get(row, "AskPrice" + number));
      const auto ask_qty =
          to_int<tc::Quantity>(columns.get(row, "AskVolume" + number));
      if (bid_price > 0 || bid_qty > 0) {
        snapshot.expected.bids[snapshot.expected.bid_count++] =
            LevelKey{bid_price, bid_qty};
      }
      if (ask_price > 0 || ask_qty > 0) {
        snapshot.expected.asks[snapshot.expected.ask_count++] =
            LevelKey{ask_price, ask_qty};
      }
    }
    snapshots.push_back(snapshot);
  }
  return snapshots;
}

[[nodiscard]] BookKey current_book_key(const ob::MboRebuilder& book,
                                       tc::Quantity total_bid_qty,
                                       tc::Quantity total_ask_qty) {
  const auto snapshot = book.snapshot<10>();
  BookKey out{
      .total_bid_qty = total_bid_qty,
      .total_ask_qty = total_ask_qty,
      .bid_count = static_cast<std::uint8_t>(snapshot.bid_count),
      .ask_count = static_cast<std::uint8_t>(snapshot.ask_count),
  };
  for (std::size_t index = 0; index < snapshot.bid_count; ++index) {
    out.bids[index] =
        LevelKey{snapshot.bids[index].price, snapshot.bids[index].quantity};
  }
  for (std::size_t index = 0; index < snapshot.ask_count; ++index) {
    out.asks[index] =
        LevelKey{snapshot.asks[index].price, snapshot.asks[index].quantity};
  }
  return out;
}

struct ApplyHealth {
  std::uint64_t unknown{};
  std::uint64_t invalid_quantity{};
  std::uint64_t invalid{};
  std::uint64_t duplicate{};
  std::uint64_t unsupported{};

  [[nodiscard]] std::uint64_t errors() const noexcept {
    return unknown + invalid_quantity + invalid + duplicate + unsupported;
  }
};

void record_apply(ob::MboApplyResult result, ApplyHealth& health) {
  switch (result.status) {
    case ob::MboApplyStatus::Accepted:
      break;
    case ob::MboApplyStatus::UnknownOrder:
      ++health.unknown;
      break;
    case ob::MboApplyStatus::InvalidQuantity:
      ++health.invalid_quantity;
      break;
    case ob::MboApplyStatus::DuplicateOrder:
      ++health.duplicate;
      break;
    case ob::MboApplyStatus::UnsupportedAction:
      ++health.unsupported;
      break;
    case ob::MboApplyStatus::InvalidEvent:
    case ob::MboApplyStatus::InvalidSide:
    case ob::MboApplyStatus::InvalidPrice:
      ++health.invalid;
      break;
  }
}

struct Result {
  std::string symbol;
  tl::Market market{tl::Market::Unknown};
  std::uint64_t events{};
  std::uint64_t snapshots{};
  std::uint64_t skipped_snapshots{};
  std::uint64_t unique{};
  std::uint64_t ambiguous{};
  std::uint64_t no_book_match{};
  std::uint64_t no_trade_anchor{};
  std::uint64_t matching_states{};
  LoadHealth load;
  ApplyHealth apply;
  std::size_t orders_left{};
};

[[nodiscard]] Result validate_symbol(const Options& options,
                                     const std::string& symbol) {
  const auto market = symbol_market(symbol);
  if (market == tl::Market::Unknown) {
    throw std::runtime_error("symbol needs .SH or .SZ suffix: " + symbol);
  }
  Result result{.symbol = symbol, .market = market};
  auto events = load_events(options, symbol, market, result.load);
  auto snapshots = load_snapshots(options, symbol, market,
                                  result.skipped_snapshots);
  result.events = events.size();
  result.snapshots = snapshots.size();

  std::unordered_map<TradeAnchor, std::vector<std::size_t>, TradeAnchorHash>
      snapshots_by_anchor;
  for (std::size_t index = 0; index < snapshots.size(); ++index) {
    snapshots_by_anchor[snapshots[index].anchor].push_back(index);
  }

  ob::MboRebuilder book;
  md::venues::TradingPhaseTracker phase_tracker;
  book.reserve_orders(std::min<std::size_t>(events.size(), 100'000));
  std::optional<md::replay::EventJournalWriter> journal;
  if (!options.journal_dir.empty()) {
    fs::create_directories(options.journal_dir);
    journal.emplace(options.journal_dir / (symbol + ".mdevt"),
                    tl::kCnStockTonglianSourceId,
                    to_int<std::uint32_t>(options.root.filename().string()));
  }
  TradeAnchor anchor{};
  auto observe = [&](tc::Sequence sequence) {
    const auto group = snapshots_by_anchor.find(anchor);
    if (group == snapshots_by_anchor.end()) {
      return;
    }
    const auto total_bid_qty = book.side_total_qty(tc::Side::Buy);
    const auto total_ask_qty = book.side_total_qty(tc::Side::Sell);
    std::optional<BookKey> key;
    for (const auto snapshot_index : group->second) {
      auto& snapshot = snapshots[snapshot_index];
      snapshot.anchor_seen = true;
      if (snapshot.expected.total_bid_qty != total_bid_qty ||
          snapshot.expected.total_ask_qty != total_ask_qty) {
        continue;
      }
      if (!key) {
        key = current_book_key(book, total_bid_qty, total_ask_qty);
      }
      if (*key == snapshot.expected) {
        ++snapshot.match_count;
        if (snapshot.first_match_sequence == 0) {
          snapshot.first_match_sequence = sequence;
        }
        snapshot.last_match_sequence = sequence;
      }
    }
  };

  observe(0);
  for (const auto& event : events) {
    if (event.payload == Payload::Order) {
      const auto& order = std::get<te::BookOrder>(event.data);
      if (journal && !journal->append(order, event.channel)) {
        throw std::runtime_error("写入标准事件日志失败");
      }
      record_apply(book.apply(order), result.apply);
    } else if (event.payload == Payload::Transaction) {
      const auto& transaction = std::get<te::BookTransaction>(event.data);
      if (journal && !journal->append(transaction, event.channel)) {
        throw std::runtime_error("写入标准事件日志失败");
      }
      record_apply(book.apply(transaction), result.apply);
      if (event.is_trade()) {
        ++anchor.count;
        anchor.volume += transaction.quantity;
        anchor.amount += transaction.price * transaction.quantity;
      }
    } else {
      // 状态事件与盘口事件共享序号和 journal，但不改变价格队列。
      const auto& status = std::get<te::Status>(event.data);
      if (journal && !journal->append(status, event.channel)) {
        throw std::runtime_error("写入标准事件日志失败");
      }
      if (phase_tracker.apply(status) !=
          md::venues::PhaseApplyStatus::Applied) {
        throw std::runtime_error("交易阶段事件顺序异常");
      }
    }
    observe(event.sequence);
  }
  result.orders_left = book.order_count();

  int mismatch_printed{};
  for (const auto& snapshot : snapshots) {
    if (!snapshot.anchor_seen) {
      ++result.no_trade_anchor;
    } else if (snapshot.match_count == 0) {
      ++result.no_book_match;
    } else if (snapshot.match_count == 1) {
      ++result.unique;
      ++result.matching_states;
    } else {
      ++result.ambiguous;
      result.matching_states += snapshot.match_count;
    }
    if (mismatch_printed < options.mismatch_limit &&
        (!snapshot.anchor_seen || snapshot.match_count == 0)) {
      ++mismatch_printed;
      std::cout << "未匹配\t" << symbol << "\ttime=" << snapshot.time
                << "\ttrades=" << snapshot.anchor.count
                << "\tvolume=" << snapshot.anchor.volume
                << "\tamount=" << snapshot.anchor.amount
                << "\tanchor=" << (snapshot.anchor_seen ? "yes" : "no")
                << '\n';
    }
  }
  return result;
}

[[nodiscard]] std::vector<std::string> split_symbols(std::string_view text) {
  std::vector<std::string> out;
  std::string current;
  for (const auto ch : text) {
    if (ch == ',') {
      if (!current.empty()) {
        out.push_back(current);
      }
      current.clear();
    } else {
      current.push_back(ch);
    }
  }
  if (!current.empty()) {
    out.push_back(current);
  }
  return out;
}

[[nodiscard]] std::optional<std::string> option_value(int& index, int argc,
                                                      char** argv) {
  const std::string argument = argv[index];
  const auto equal = argument.find('=');
  if (equal != std::string::npos) {
    return argument.substr(equal + 1);
  }
  if (++index >= argc) {
    return std::nullopt;
  }
  return argv[index];
}

void usage(const char* program) {
  std::cerr << "用法: " << program
            << " ROOT [--symbols A.SH,B.SZ] [--market ALL|SH|SZ]"
               " [--limit N] [--mismatch-limit N] [--journal-dir DIR]"
               " [--summary]\n";
}

[[nodiscard]] bool parse_options(int argc, char** argv, Options& options) {
  if (argc < 2 || std::string_view(argv[1]) == "--help") {
    return false;
  }
  options.root = argv[1];
  for (int index = 2; index < argc; ++index) {
    const std::string name = std::string(argv[index]).substr(
        0, std::string(argv[index]).find('='));
    if (name == "--symbols") {
      const auto value = option_value(index, argc, argv);
      if (!value) {
        return false;
      }
      options.symbols = split_symbols(*value);
    } else if (name == "--market") {
      const auto value = option_value(index, argc, argv);
      if (!value) {
        return false;
      }
      options.market = *value == "SH"    ? tl::Market::Shanghai
                       : *value == "SZ"  ? tl::Market::Shenzhen
                       : *value == "ALL" ? tl::Market::Unknown
                                          : static_cast<tl::Market>(255);
      if (static_cast<int>(options.market) == 255) {
        return false;
      }
    } else if (name == "--limit") {
      const auto value = option_value(index, argc, argv);
      if (!value) {
        return false;
      }
      options.limit = to_int<int>(*value);
    } else if (name == "--mismatch-limit") {
      const auto value = option_value(index, argc, argv);
      if (!value) {
        return false;
      }
      options.mismatch_limit = to_int<int>(*value);
    } else if (name == "--journal-dir") {
      const auto value = option_value(index, argc, argv);
      if (!value) {
        return false;
      }
      options.journal_dir = *value;
    } else if (name == "--summary") {
      options.summary = true;
    } else {
      return false;
    }
  }
  return true;
}

void append_symbols(const Options& options, tl::Market market,
                    std::vector<std::string>& symbols,
                    std::unordered_set<std::string>& seen) {
  const auto path = snapshot_path(options.root, market);
  if (!fs::exists(path)) {
    return;
  }
  ParquetRows rows(path, std::nullopt);
  std::string symbol;
  while (rows.getline(symbol)) {
    symbol = trim(symbol);
    if (symbol.empty()) {
      continue;
    }
    symbol += market == tl::Market::Shanghai ? ".SH" : ".SZ";
    if (seen.insert(symbol).second) {
      symbols.push_back(symbol);
    }
    if (options.limit > 0 &&
        symbols.size() >= static_cast<std::size_t>(options.limit)) {
      return;
    }
  }
}

[[nodiscard]] std::vector<std::string> selected_symbols(
    const Options& options) {
  if (!options.symbols.empty()) {
    return options.symbols;
  }
  std::vector<std::string> symbols;
  std::unordered_set<std::string> seen;
  if (options.market != tl::Market::Shenzhen) {
    append_symbols(options, tl::Market::Shanghai, symbols, seen);
  }
  if ((options.limit <= 0 ||
       symbols.size() < static_cast<std::size_t>(options.limit)) &&
      options.market != tl::Market::Shanghai) {
    append_symbols(options, tl::Market::Shenzhen, symbols, seen);
  }
  return symbols;
}

void print_result(const Result& result) {
  std::cout << result.symbol << '\t' << market_name(result.market) << '\t'
            << result.events << '\t' << result.snapshots << '\t'
            << result.unique << '\t' << result.ambiguous << '\t'
            << result.no_book_match << '\t' << result.no_trade_anchor << '\t'
            << result.matching_states << '\t' << result.apply.errors() << '\t'
            << result.load.sequence.channel << '\t'
            << result.load.sequence.first << '\t' << result.load.sequence.last
            << '\t' << result.load.sequence.duplicates << '\t'
            << result.load.sequence.regressions << '\t' << result.orders_left
            << '\n';
}

void print_summary(const std::vector<Result>& results) {
  Result total{};
  for (const auto& result : results) {
    total.events += result.events;
    total.snapshots += result.snapshots;
    total.skipped_snapshots += result.skipped_snapshots;
    total.unique += result.unique;
    total.ambiguous += result.ambiguous;
    total.no_book_match += result.no_book_match;
    total.no_trade_anchor += result.no_trade_anchor;
    total.matching_states += result.matching_states;
    total.apply.unknown += result.apply.unknown;
    total.apply.invalid_quantity += result.apply.invalid_quantity;
    total.apply.invalid += result.apply.invalid;
    total.apply.duplicate += result.apply.duplicate;
    total.apply.unsupported += result.apply.unsupported;
    total.load.invalid_rows += result.load.invalid_rows;
    total.load.unsupported_rows += result.load.unsupported_rows;
  }
  std::cout << "\n汇总\n"
            << "标的数\t" << results.size() << '\n'
            << "事件数\t" << total.events << '\n'
            << "连续竞价快照\t" << total.snapshots << '\n'
            << "跳过非连续竞价快照\t" << total.skipped_snapshots << '\n'
            << "UniqueWindowMatch\t" << total.unique << '\n'
            << "AmbiguousWindowMatch\t" << total.ambiguous << '\n'
            << "NoBookMatch\t" << total.no_book_match << '\n'
            << "NoTradeAnchor\t" << total.no_trade_anchor << '\n'
            << "候选匹配状态数\t" << total.matching_states << '\n'
            << "映射异常\t" << total.load.invalid_rows << '/'
            << total.load.unsupported_rows << '\n'
            << "应用异常\tunknown=" << total.apply.unknown
            << "\tbad_qty=" << total.apply.invalid_quantity
            << "\tinvalid=" << total.apply.invalid
            << "\tduplicate=" << total.apply.duplicate
            << "\tunsupported=" << total.apply.unsupported << '\n';
}

}  // namespace

int book_validate_main(int argc, char** argv) {
  Options options{};
  if (!parse_options(argc, argv, options)) {
    usage(argv[0]);
    return argc >= 2 && std::string_view(argv[1]) == "--help" ? 0 : 2;
  }
  try {
    const auto symbols = selected_symbols(options);
    std::cout << "代码\t市场\t事件数\t连续快照\tUnique\tAmbiguous"
                 "\tNoBookMatch\tNoTradeAnchor\t匹配状态数\t应用异常"
                 "\t通道\t首序号\t末序号\t重复\t倒退\t剩余订单\n";
    std::vector<Result> results;
    results.reserve(symbols.size());
    for (const auto& symbol : symbols) {
      auto result = validate_symbol(options, symbol);
      print_result(result);
      results.push_back(std::move(result));
    }
    if (options.summary) {
      print_summary(results);
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}

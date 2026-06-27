#include "binance_live_options.hpp"

#include "cli_options.hpp"

#include <array>

namespace md::apps::md_node {
namespace {

struct FeedKindCliEntry {
  std::string_view name;
  md::core::FeedKind kind;
  bool supports_normalize{};
};

inline constexpr auto kFeedKindCliEntries =
    std::to_array<FeedKindCliEntry>({
        {"trade", md::core::FeedKind::Trade, true},
        {"aggTrade", md::core::FeedKind::AggregateTrade, false},
        {"bookTicker", md::core::FeedKind::TopOfBook, true},
        {"depth", md::core::FeedKind::BookUpdate, false},
        {"depth20", md::core::FeedKind::BookSnapshot, false},
    });

[[nodiscard]] bool parse_feed_kind(std::string_view text,
                                   md::core::FeedKind &feed_kind) noexcept {
  for (const auto &entry : kFeedKindCliEntries) {
    if (text == entry.name) {
      feed_kind = entry.kind;
      return true;
    }
  }
  return false;
}

[[nodiscard]] std::vector<std::string> parse_symbol_list(std::string_view text) {
  std::vector<std::string> symbols{};
  std::size_t start = 0;
  while (start <= text.size()) {
    const auto comma = text.find(',', start);
    const auto end = comma == std::string_view::npos ? text.size() : comma;
    if (end > start) {
      symbols.emplace_back(text.substr(start, end - start));
    }
    if (comma == std::string_view::npos) {
      break;
    }
    start = comma + 1;
  }
  return symbols;
}

} // 匿名命名空间

std::string binance_feed_kind_usage(bool normalize_only) {
  std::string usage{};
  for (const auto &entry : kFeedKindCliEntries) {
    if (normalize_only && !entry.supports_normalize) {
      continue;
    }
    if (!usage.empty()) {
      usage += '|';
    }
    usage += entry.name;
  }
  return usage;
}

BinanceLiveArgs parse_binance_live_args(int argc, char **argv) {
  BinanceLiveArgs args{};
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg{argv[i]};
    if (const auto value = option_value(arg, "--symbol="); !value.empty()) {
      args.symbols = {std::string(value)};
      continue;
    }
    if (const auto value = option_value(arg, "--symbols="); !value.empty()) {
      const auto symbols = parse_symbol_list(value);
      if (!symbols.empty()) {
        args.symbols = symbols;
      }
      continue;
    }
    if (const auto value = option_value(arg, "--feed="); !value.empty()) {
      md::core::FeedKind feed_kind{};
      if (parse_feed_kind(value, feed_kind)) {
        args.feed_kind = feed_kind;
      }
      continue;
    }
    if (const auto value = option_value(arg, "--messages="); !value.empty()) {
      std::uint64_t max_messages = 0;
      if (parse_u64(value, max_messages)) {
        args.max_messages = max_messages;
      }
      continue;
    }
    if (arg == "--normalize") {
      args.normalize = true;
      continue;
    }
    if (arg == "--log-payload") {
      args.log_payload = true;
      args.log_payloads = 5;
      continue;
    }
    if (const auto value = option_value(arg, "--log-payload=");
        !value.empty()) {
      std::uint64_t log_payloads = 0;
      if (parse_u64(value, log_payloads)) {
        args.log_payload = true;
        args.log_payloads = log_payloads;
      }
      continue;
    }
    if (arg == "--log-normalized") {
      args.normalize = true;
      args.log_normalized = true;
      args.log_normalized_events = 5;
      continue;
    }
    if (const auto value = option_value(arg, "--log-normalized=");
        !value.empty()) {
      std::uint64_t log_normalized_events = 0;
      if (parse_u64(value, log_normalized_events)) {
        args.normalize = true;
        args.log_normalized = true;
        args.log_normalized_events = log_normalized_events;
      }
      continue;
    }
    if (const auto value = option_value(arg, "--payload-bytes=");
        !value.empty()) {
      std::uint64_t payload_preview_bytes = 0;
      if (parse_u64(value, payload_preview_bytes)) {
        args.payload_preview_bytes =
            static_cast<std::size_t>(payload_preview_bytes);
      }
      continue;
    }
    if (arg == "--reconnect") {
      args.max_attempts = 0;
      continue;
    }
    if (arg == "--reconnect-completed") {
      args.reconnect_on_completed = true;
      continue;
    }
    if (const auto value = option_value(arg, "--max-attempts=");
        !value.empty()) {
      std::uint64_t max_attempts = 0;
      if (parse_u64(value, max_attempts)) {
        args.max_attempts = static_cast<std::uint32_t>(max_attempts);
      }
      continue;
    }
    if (const auto value = option_value(arg, "--backoff-ms=");
        !value.empty()) {
      std::uint64_t backoff_ms = 0;
      if (parse_u64(value, backoff_ms)) {
        args.backoff_ms = backoff_ms;
      }
      continue;
    }
    if (const auto value = option_value(arg, "--max-backoff-ms=");
        !value.empty()) {
      std::uint64_t max_backoff_ms = 0;
      if (parse_u64(value, max_backoff_ms)) {
        args.max_backoff_ms = max_backoff_ms;
      }
      continue;
    }
    if (const auto value = option_value(arg, "--idle-timeout-ms=");
        !value.empty()) {
      std::uint64_t idle_timeout_ms = 0;
      if (parse_u64(value, idle_timeout_ms)) {
        args.idle_timeout_ms = idle_timeout_ms;
      }
      continue;
    }
  }
  return args;
}

} // 命名空间 md::apps::md_node

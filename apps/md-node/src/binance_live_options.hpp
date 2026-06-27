#pragma once

#include "md/core/feed.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace md::apps::md_node {

struct BinanceLiveArgs {
  std::vector<std::string> symbols{"BTCUSDT"};
  md::core::FeedKind feed_kind{md::core::FeedKind::TopOfBook};
  std::uint64_t max_messages{250};
  std::uint64_t log_payloads{};
  std::uint64_t log_normalized_events{};
  std::size_t payload_preview_bytes{512};
  std::uint32_t max_attempts{1};
  std::uint64_t backoff_ms{250};
  std::uint64_t max_backoff_ms{30000};
  std::uint64_t idle_timeout_ms{30000};
  bool normalize{};
  bool log_payload{};
  bool log_normalized{};
  bool reconnect_on_completed{};
};

[[nodiscard]] BinanceLiveArgs parse_binance_live_args(int argc, char **argv);
[[nodiscard]] std::string binance_feed_kind_usage(bool normalize_only);

} // 命名空间 md::apps::md_node

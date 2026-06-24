#include "md/adapters/crypto/binance/feed_client.hpp"
#include "md/service/feed_message_handler.hpp"
#include "md/service/feed_session.hpp"
#include "md/runtime/logging.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <ctime>
#include <string>
#include <string_view>
#include <vector>

namespace {

using FeedKind = md::core::FeedKind;

struct BinanceLivePreviewArgs {
  std::vector<std::string> symbols{"BTCUSDT"};
  FeedKind feed_kind{FeedKind::TopOfBook};
  std::uint64_t max_messages{250};
  std::uint64_t log_payloads{};
  std::size_t payload_preview_bytes{512};
  std::uint32_t max_attempts{1};
  std::uint64_t backoff_ms{250};
  std::uint64_t max_backoff_ms{30000};
  std::uint64_t idle_timeout_ms{30000};
  bool log_payload{};
  bool reconnect_on_completed{};
};

std::atomic_bool g_stop_requested{false};

[[nodiscard]] std::string_view option_value(std::string_view arg,
                                            std::string_view name) noexcept {
  if (!arg.starts_with(name)) {
    return {};
  }
  return arg.substr(name.size());
}

[[nodiscard]] bool parse_u64(std::string_view text,
                             std::uint64_t &value) noexcept {
  if (text.empty()) {
    return false;
  }
  std::uint64_t parsed = 0;
  for (char ch : text) {
    if (ch < '0' || ch > '9') {
      return false;
    }
    parsed = (parsed * 10) + static_cast<std::uint64_t>(ch - '0');
  }
  value = parsed;
  return true;
}

[[nodiscard]] bool parse_log_level(std::string_view text,
                                   md::runtime::LogLevel &level) noexcept {
  if (text == "trace") {
    level = md::runtime::LogLevel::Trace;
    return true;
  }
  if (text == "debug") {
    level = md::runtime::LogLevel::Debug;
    return true;
  }
  if (text == "info") {
    level = md::runtime::LogLevel::Info;
    return true;
  }
  if (text == "warn") {
    level = md::runtime::LogLevel::Warn;
    return true;
  }
  if (text == "error") {
    level = md::runtime::LogLevel::Error;
    return true;
  }
  if (text == "off") {
    level = md::runtime::LogLevel::Off;
    return true;
  }
  return false;
}

[[nodiscard]] md::runtime::LogConfig parse_log_config(int argc,
                                                       char **argv) noexcept {
  md::runtime::LogConfig config{};
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg{argv[i]};
    if (const auto value = option_value(arg, "--log-level="); !value.empty()) {
      md::runtime::LogLevel level{};
      if (parse_log_level(value, level)) {
        config.level = level;
      }
    }
  }
  return config;
}

[[nodiscard]] bool parse_feed_kind(std::string_view text,
                                   FeedKind &feed_kind) noexcept {
  if (text == "trade") {
    feed_kind = FeedKind::Trade;
    return true;
  }
  if (text == "aggTrade") {
    feed_kind = FeedKind::AggregateTrade;
    return true;
  }
  if (text == "bookTicker") {
    feed_kind = FeedKind::TopOfBook;
    return true;
  }
  if (text == "depth") {
    feed_kind = FeedKind::BookUpdate;
    return true;
  }
  if (text == "depth20") {
    feed_kind = FeedKind::BookSnapshot;
    return true;
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

[[nodiscard]] BinanceLivePreviewArgs parse_live_preview_args(int argc,
                                                             char **argv) {
  BinanceLivePreviewArgs args{};
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
      FeedKind feed_kind{};
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

void request_stop(int) noexcept {
  g_stop_requested.store(true, std::memory_order_relaxed);
}

[[nodiscard]] md::service::FeedRunAttemptResult to_feed_attempt_result(
    const md::adapters::crypto::binance::BinanceFeedClientRunResult
        &run_result) {
  md::service::FeedRunAttemptResult attempt{};
  attempt.status = md::service::to_feed_run_status(run_result.websocket.status);
  attempt.error_class = md::service::classify_feed_run_status(attempt.status);
  attempt.messages = run_result.messages;
  attempt.bytes = run_result.bytes;
  attempt.started_ns = run_result.websocket.started_ns;
  attempt.ended_ns = run_result.websocket.ended_ns;
  attempt.first_recv_ts_ns = run_result.first_recv_ts_ns;
  attempt.last_recv_ts_ns = run_result.last_recv_ts_ns;
  attempt.error = run_result.websocket.error;
  return attempt;
}

} // namespace

int main(int argc, char** argv) {
    namespace binance = md::adapters::crypto::binance;

    md::runtime::init_logging(parse_log_config(argc, argv));

    for (int i = 1; i < argc; ++i) {
        if (std::string_view{argv[i]} == "--binance-feed-spec-preview") {
            binance::BinanceFeedSpec spec{};
            spec.market = md::core::MarketSegment::Spot;
            spec.symbols = {"BTCUSDT"};
            spec.kind = md::core::FeedKind::Trade;
            const auto connection = binance::make_connection_spec(
                1, binance::BinanceEnvironment::MarketDataOnly, spec);
            const auto &feed = connection.feeds.front();
            MDF_LOG_INFO("binance feed spec preview");
            MDF_LOG_INFO("provider_feed_key={}", feed.provider_feed_key);
            MDF_LOG_INFO("endpoint={}", connection.endpoint);
            MDF_LOG_INFO("feed_id={}", feed.feed_id);
            return 0;
        }
        const std::string_view command{argv[i]};
        if (command == "--binance-live" ||
            command == "--binance-live-preview") {
            const auto args = parse_live_preview_args(argc, argv);
            binance::BinanceFeedSpec spec{};
            spec.market = md::core::MarketSegment::Spot;
            spec.symbols = args.symbols;
            spec.kind = args.feed_kind;
            if (spec.kind == md::core::FeedKind::BookSnapshot) {
                spec.depth = md::core::FeedDepth::Top20;
            }
            const auto connection = binance::make_connection_spec(
                1, binance::BinanceEnvironment::MarketDataOnly, spec);

            binance::BinanceFeedClientOptions options{};
            options.websocket.max_message_bytes = md::net::kDefaultWsMessageBytes;
            options.websocket.idle_timeout =
                std::chrono::milliseconds{args.idle_timeout_ms};

            md::service::FeedSessionOptions session_options{};
            session_options.max_messages = args.max_messages;
            session_options.retry.max_attempts = args.max_attempts;
            session_options.retry.initial_backoff =
                std::chrono::milliseconds{args.backoff_ms};
            session_options.retry.max_backoff =
                std::chrono::milliseconds{args.max_backoff_ms};
            session_options.retry.reconnect_on_completed =
                args.reconnect_on_completed;

            MDF_LOG_INFO(
                "binance live feed backend=Boost.Beast{}",
                md::net::beast::kWebSocketBackendAvailable ? ""
                                                            : " unavailable");
            for (const auto &feed : connection.feeds) {
                MDF_LOG_INFO("provider_feed_key={}", feed.provider_feed_key);
                MDF_LOG_INFO("feed_id={}", feed.feed_id);
            }
            MDF_LOG_INFO("endpoint={}", connection.endpoint);
            MDF_LOG_INFO("feeds={}", connection.feeds.size());
            MDF_LOG_INFO("max_messages={}", session_options.max_messages);
            MDF_LOG_INFO("max_attempts={}",
                         session_options.retry.max_attempts);
            MDF_LOG_INFO("backoff_ms={}",
                         session_options.retry.initial_backoff.count());
            MDF_LOG_INFO("max_backoff_ms={}",
                         session_options.retry.max_backoff.count());
            MDF_LOG_INFO(
                "reconnect_on_completed={}",
                session_options.retry.reconnect_on_completed ? "true" : "false");
            MDF_LOG_INFO("idle_timeout_ms={}",
                         options.websocket.idle_timeout.count());
            if (args.log_payload) {
                MDF_LOG_INFO("log_payloads={}", args.log_payloads);
                MDF_LOG_INFO("payload_preview_bytes={}",
                             args.payload_preview_bytes);
            }

            md::service::FeedMessageLogOptions log_options{};
            log_options.max_payloads = args.log_payloads;
            log_options.max_payload_bytes = args.payload_preview_bytes;
            log_options.log_metadata = args.log_payload;
            log_options.log_payload = args.log_payload;
            md::service::FeedMessageLogHandler log_handler{log_options};
            md::core::FeedMessageHandler handler = nullptr;
            void *handler_data = nullptr;
            if (args.log_payload) {
                handler = &md::service::FeedMessageLogHandler::handle;
                handler_data = &log_handler;
            }

            std::signal(SIGINT, &request_stop);
            std::signal(SIGTERM, &request_stop);
            md::service::FeedStopToken stop_token{.requested = &g_stop_requested};

            const auto cpu_started = std::clock();
            const auto result = md::service::run_feed_session(
                session_options,
                [&](const md::service::FeedRunAttemptContext &attempt_context) {
                    auto attempt_options = options;
                    attempt_options.max_messages = attempt_context.max_messages;
                    binance::BinanceFeedClient client{connection,
                                                      attempt_options};
                    return to_feed_attempt_result(
                        client.run(attempt_context.handler,
                                   attempt_context.user_data));
                },
                handler, handler_data, stop_token);
            const auto cpu_ended = std::clock();

            const auto active_ns =
                result.stats.last_recv_ts_ns > result.stats.first_recv_ts_ns
                    ? result.stats.last_recv_ts_ns - result.stats.first_recv_ts_ns
                    : 0;
            const double active_sec =
                static_cast<double>(active_ns) / 1'000'000'000.0;
            const double active_msg_per_sec =
                active_sec > 0.0 && result.stats.messages > 1
                    ? static_cast<double>(result.stats.messages - 1) / active_sec
                    : 0.0;
            const double avg_bytes =
                result.stats.messages > 0
                    ? static_cast<double>(result.stats.bytes) /
                          static_cast<double>(result.stats.messages)
                    : 0.0;
            const double cpu_sec =
                static_cast<double>(cpu_ended - cpu_started) /
                static_cast<double>(CLOCKS_PER_SEC);
            const double cpu_us_per_msg =
                result.stats.messages > 0
                    ? (cpu_sec * 1'000'000.0) /
                          static_cast<double>(result.stats.messages)
                    : 0.0;

            MDF_LOG_INFO(
                "session_stop_reason={}",
                md::service::feed_session_stop_reason_name(result.stop_reason));
            MDF_LOG_INFO(
                "last_status={}",
                md::service::feed_run_status_name(result.stats.last_status));
            MDF_LOG_INFO("last_error_class={}",
                         md::service::feed_error_class_name(
                             result.stats.last_error_class));
            if (!result.stats.last_error.empty()) {
                MDF_LOG_WARN("last_error={}", result.stats.last_error);
            }
            MDF_LOG_INFO("attempts={}", result.stats.attempts);
            MDF_LOG_INFO("reconnects={}", result.stats.reconnects);
            MDF_LOG_INFO("messages={}", result.stats.messages);
            MDF_LOG_INFO("bytes={}", result.stats.bytes);
            MDF_LOG_INFO("active_sec={}", active_sec);
            MDF_LOG_INFO("active_msg_per_sec={}", active_msg_per_sec);
            MDF_LOG_INFO("cpu_sec={}", cpu_sec);
            MDF_LOG_INFO("cpu_us_per_msg={}", cpu_us_per_msg);
            MDF_LOG_INFO("avg_bytes={}", avg_bytes);
            if (args.log_payload) {
                MDF_LOG_INFO("logged_payloads={}", log_handler.logged());
            }
            return result.ok() ? 0 : 1;
        }
    }

    MDF_LOG_INFO("md-node skeleton");
    MDF_LOG_INFO("roles: gateway, controller, or gateway,controller");
    MDF_LOG_INFO("preview: --binance-feed-spec-preview");
    MDF_LOG_INFO(
        "live feed: --binance-live "
        "[--symbol=BTCUSDT | --symbols=BTCUSDT,ETHUSDT | --symbols=ALL] "
        "[--feed=bookTicker] [--messages=250] "
        "[--log-payload[=5]] [--payload-bytes=512] "
        "[--max-attempts=1 | --reconnect] "
        "[--backoff-ms=250] [--max-backoff-ms=30000] "
        "[--reconnect-completed] [--idle-timeout-ms=30000] "
        "[--log-level=info]");
    std::string args_line{"args:"};
    for (int i = 1; i < argc; ++i) {
        args_line += ' ';
        args_line += argv[i];
    }
    MDF_LOG_INFO("{}", args_line);
    return 0;
}

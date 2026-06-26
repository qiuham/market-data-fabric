#include "md/adapters/crypto/binance/book_ticker_mapper.hpp"
#include "md/adapters/crypto/binance/websocket_feed_client.hpp"
#include "md/adapters/crypto/binance/trade_mapper.hpp"
#include "md/service/feed_message_handler.hpp"
#include "md/service/feed_payload_log_handler.hpp"
#include "md/service/feed_session.hpp"
#include "md/service/mapper_stats.hpp"
#include "md/runtime/logging.hpp"

#include <array>
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

struct FeedKindCliEntry {
  std::string_view name;
  FeedKind kind;
  bool supports_normalize{};
};

inline constexpr auto kFeedKindCliEntries =
    std::to_array<FeedKindCliEntry>({
        {"trade", FeedKind::Trade, true},
        {"aggTrade", FeedKind::AggregateTrade, false},
        {"bookTicker", FeedKind::TopOfBook, true},
        {"depth", FeedKind::BookUpdate, false},
        {"depth20", FeedKind::BookSnapshot, false},
    });

struct BinanceLiveArgs {
  std::vector<std::string> symbols{"BTCUSDT"};
  FeedKind feed_kind{FeedKind::TopOfBook};
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
  for (const auto &entry : kFeedKindCliEntries) {
    if (text == entry.name) {
      feed_kind = entry.kind;
      return true;
    }
  }
  return false;
}

[[nodiscard]] std::string feed_kind_usage(bool normalize_only) {
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

[[nodiscard]] std::string_view
feed_key_label(const md::core::FeedConnectionSpec &connection) noexcept {
  if (connection.feeds.empty()) {
    return "";
  }
  if (connection.feeds.size() != 1) {
    return "multiple";
  }
  return connection.feeds.front().provider_feed_key;
}

[[nodiscard]] const char *
aggressor_side_name(md::core::AggressorSide side) noexcept {
  switch (side) {
  case md::core::AggressorSide::Buy:
    return "buy";
  case md::core::AggressorSide::Sell:
    return "sell";
  case md::core::AggressorSide::None:
    return "none";
  case md::core::AggressorSide::Unknown:
    return "unknown";
  }
  return "unknown";
}

[[nodiscard]] std::uint32_t
make_binance_spot_instrument_id(std::string_view provider_symbol) {
  const auto seed = "binance:spot:" + std::string(provider_symbol);
  return md::core::stable_id32(seed);
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

[[nodiscard]] BinanceLiveArgs parse_binance_live_args(int argc, char **argv) {
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

struct BinanceQuoteMapperHandler {
  const md::adapters::crypto::binance::BinanceBookTickerMappingContext
      *context{};
  md::service::MapperRuntimeStats *stats{};
  std::uint64_t max_logged_events{};
  std::uint64_t logged_events{};
  bool log_events{};

  bool on_message(const md::core::FeedMessageView &message) noexcept {
    md::core::Quote quote{};
    if (context == nullptr ||
        !md::adapters::crypto::binance::map_book_ticker_to_quote(
            message, *context, quote)) {
      if (stats != nullptr) {
        stats->observe_failure(message);
      }
      if (stats != nullptr && stats->failures <= 3) {
        MDF_LOG_WARN("Quote 标准化失败 feed_id={} payload_size={} "
                     "recv_ts_ns={}",
                     message.envelope.feed_id, message.envelope.payload_size,
                     message.envelope.recv_ts_ns);
      }
      return true;
    }

    if (stats != nullptr) {
      stats->observe_success(message);
    }
    if (!log_events ||
        (max_logged_events != 0 && logged_events >= max_logged_events)) {
      return true;
    }

    ++logged_events;
    MDF_LOG_INFO(
        "Quote 标准化结果 symbol={} exchange_seq={} bid_price={} "
        "bid_quantity={} ask_price={} ask_quantity={} recv_ts_ns={}",
        context->provider_symbol, quote.header.exchange_seq, quote.bid_price,
        quote.bid_quantity, quote.ask_price, quote.ask_quantity,
        quote.header.recv_ts_ns);
    return true;
  }
};

struct BinanceTradeMapperHandler {
  const md::adapters::crypto::binance::BinanceTradeMappingContext *context{};
  md::service::MapperRuntimeStats *stats{};
  std::uint64_t max_logged_events{};
  std::uint64_t logged_events{};
  bool log_events{};

  bool on_message(const md::core::FeedMessageView &message) noexcept {
    md::core::Trade trade{};
    if (context == nullptr ||
        !md::adapters::crypto::binance::map_trade_to_trade(message, *context,
                                                           trade)) {
      if (stats != nullptr) {
        stats->observe_failure(message);
      }
      if (stats != nullptr && stats->failures <= 3) {
        MDF_LOG_WARN("Trade 标准化失败 feed_id={} payload_size={} "
                     "recv_ts_ns={}",
                     message.envelope.feed_id, message.envelope.payload_size,
                     message.envelope.recv_ts_ns);
      }
      return true;
    }

    if (stats != nullptr) {
      stats->observe_success(message);
    }
    if (!log_events ||
        (max_logged_events != 0 && logged_events >= max_logged_events)) {
      return true;
    }

    ++logged_events;
    MDF_LOG_INFO(
        "Trade 标准化结果 symbol={} trade_id={} price={} quantity={} aggressor_side={} "
        "source_ts_ns={} recv_ts_ns={}",
        context->provider_symbol, trade.trade_id, trade.price, trade.quantity,
        aggressor_side_name(trade.aggressor_side), trade.header.source_ts_ns,
        trade.header.recv_ts_ns);
    return true;
  }
};

struct FeedMessageHandlerChain {
  md::service::FeedPayloadLogHandler *payload_handler{};
  BinanceQuoteMapperHandler *quote_mapper_handler{};
  BinanceTradeMapperHandler *trade_mapper_handler{};

  static bool handle(const md::core::FeedMessageView &message,
                     void *user_data) noexcept {
    auto *chain = static_cast<FeedMessageHandlerChain *>(user_data);
    if (chain == nullptr) {
      return true;
    }

    if (chain->payload_handler != nullptr &&
        !chain->payload_handler->on_message(message)) {
      return false;
    }
    if (chain->quote_mapper_handler != nullptr &&
        !chain->quote_mapper_handler->on_message(message)) {
      return false;
    }
    if (chain->trade_mapper_handler != nullptr &&
        !chain->trade_mapper_handler->on_message(message)) {
      return false;
    }
    return true;
  }
};

void request_stop(int) noexcept {
  g_stop_requested.store(true, std::memory_order_relaxed);
}

[[nodiscard]] md::service::FeedRunAttemptResult to_feed_attempt_result(
    const md::adapters::crypto::binance::BinanceWebSocketFeedClientRunResult
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

} // 匿名命名空间

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
            MDF_LOG_INFO("Binance feed 配置预览");
            MDF_LOG_INFO("provider_feed_key={}", feed.provider_feed_key);
            MDF_LOG_INFO("endpoint={}", connection.endpoint);
            MDF_LOG_INFO("feed_id={}", feed.feed_id);
            return 0;
        }
        const std::string_view command{argv[i]};
        if (command == "--binance-live" ||
            command == "--binance-live-preview") {
            const auto args = parse_binance_live_args(argc, argv);
            binance::BinanceFeedSpec spec{};
            spec.market = md::core::MarketSegment::Spot;
            spec.symbols = args.symbols;
            spec.kind = args.feed_kind;
            if (spec.kind == md::core::FeedKind::BookSnapshot) {
                spec.depth = md::core::FeedDepth::Top20;
            }
            const auto connection = binance::make_connection_spec(
                1, binance::BinanceEnvironment::MarketDataOnly, spec);

            binance::BinanceWebSocketFeedClientOptions options{};
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

            MDF_LOG_INFO("行情启动 venue=binance feeds={} key={} "
                         "max_messages={} normalize={} payload_log={} "
                         "normalized_log={}",
                         connection.feeds.size(), feed_key_label(connection),
                         session_options.max_messages,
                         args.normalize ? "true" : "false",
                         args.log_payload ? args.log_payloads : 0,
                         args.log_normalized ? args.log_normalized_events : 0);
            MDF_LOG_DEBUG(
                "行情配置 backend=Boost.Beast{} endpoint={} max_attempts={} "
                "backoff_ms={} max_backoff_ms={} reconnect_on_completed={} "
                "idle_timeout_ms={} payload_preview_bytes={}",
                md::net::beast::kWebSocketBackendAvailable ? "" : " 不可用",
                connection.endpoint, session_options.retry.max_attempts,
                session_options.retry.initial_backoff.count(),
                session_options.retry.max_backoff.count(),
                session_options.retry.reconnect_on_completed ? "true" : "false",
                options.websocket.idle_timeout.count(),
                args.payload_preview_bytes);
            for (const auto &feed : connection.feeds) {
                MDF_LOG_DEBUG("feed 已解析 key={} feed_id={}",
                              feed.provider_feed_key, feed.feed_id);
            }

            md::service::FeedPayloadLogOptions payload_log_options{};
            payload_log_options.max_payloads = args.log_payloads;
            payload_log_options.max_payload_bytes = args.payload_preview_bytes;
            payload_log_options.log_metadata = args.log_payload;
            payload_log_options.log_payload = args.log_payload;
            md::service::FeedPayloadLogHandler payload_log_handler{
                payload_log_options};
            binance::BinanceBookTickerMappingContext quote_context{};
            binance::BinanceTradeMappingContext trade_context{};
            md::service::MapperRuntimeStats mapper_stats{};
            BinanceQuoteMapperHandler quote_mapper_handler{};
            BinanceTradeMapperHandler trade_mapper_handler{};
            if (args.normalize) {
                if (connection.feeds.size() != 1 ||
                    connection.feeds.front().provider_symbol.empty()) {
                    MDF_LOG_WARN(
                        "标准化当前要求只订阅一个 Binance symbol feed");
                } else {
                    const auto &feed = connection.feeds.front();
                    const auto instrument_id =
                        make_binance_spot_instrument_id(feed.provider_symbol);
                    if (spec.kind == md::core::FeedKind::TopOfBook) {
                        quote_context.instrument_id = instrument_id;
                        quote_context.provider_symbol = feed.provider_symbol;
                        quote_mapper_handler.context = &quote_context;
                        quote_mapper_handler.stats = &mapper_stats;
                        quote_mapper_handler.max_logged_events =
                            args.log_normalized_events;
                        quote_mapper_handler.log_events = args.log_normalized;
                    } else if (spec.kind == md::core::FeedKind::Trade) {
                        trade_context.instrument_id = instrument_id;
                        trade_context.provider_symbol = feed.provider_symbol;
                        trade_mapper_handler.context = &trade_context;
                        trade_mapper_handler.stats = &mapper_stats;
                        trade_mapper_handler.max_logged_events =
                            args.log_normalized_events;
                        trade_mapper_handler.log_events = args.log_normalized;
                    } else {
                        MDF_LOG_WARN("标准化当前只支持 Binance bookTicker 和 trade");
                    }
                }
            }
            FeedMessageHandlerChain handler_chain{};
            handler_chain.payload_handler =
                args.log_payload ? &payload_log_handler : nullptr;
            handler_chain.quote_mapper_handler =
                quote_mapper_handler.context != nullptr ? &quote_mapper_handler
                                                       : nullptr;
            handler_chain.trade_mapper_handler =
                trade_mapper_handler.context != nullptr ? &trade_mapper_handler
                                                       : nullptr;
            md::core::FeedMessageHandler handler = nullptr;
            void *handler_data = nullptr;
            if (handler_chain.payload_handler != nullptr ||
                handler_chain.quote_mapper_handler != nullptr ||
                handler_chain.trade_mapper_handler != nullptr) {
                handler = &FeedMessageHandlerChain::handle;
                handler_data = &handler_chain;
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
                    binance::BinanceWebSocketFeedClient client{
                        connection, attempt_options};
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

            if (args.log_payload || args.normalize) {
                MDF_LOG_INFO(
                    "行情停止 reason={} status={} messages={} bytes={} "
                    "payloads={} mapped={} map_failures={}",
                    md::service::feed_session_stop_reason_name(
                        result.stop_reason),
                    md::service::feed_run_status_name(result.stats.last_status),
                    result.stats.messages, result.stats.bytes,
                    args.log_payload ? payload_log_handler.logged() : 0,
                    args.normalize ? mapper_stats.output_events : 0,
                    args.normalize ? mapper_stats.failures : 0);
            } else {
                MDF_LOG_INFO(
                    "行情停止 reason={} status={} messages={} bytes={}",
                    md::service::feed_session_stop_reason_name(
                        result.stop_reason),
                    md::service::feed_run_status_name(result.stats.last_status),
                    result.stats.messages, result.stats.bytes);
            }
            if (!result.stats.last_error.empty()) {
                MDF_LOG_WARN("最后错误 last_error={}", result.stats.last_error);
            }
            MDF_LOG_DEBUG(
                "行情统计 attempts={} reconnects={} error_class={} "
                "active_sec={} active_msg_per_sec={} cpu_sec={} "
                "cpu_us_per_msg={} avg_bytes={}",
                result.stats.attempts, result.stats.reconnects,
                md::service::feed_error_class_name(
                    result.stats.last_error_class),
                active_sec, active_msg_per_sec, cpu_sec, cpu_us_per_msg,
                avg_bytes);
            return result.ok() ? 0 : 1;
        }
    }

    MDF_LOG_INFO("md-node 行情节点");
    MDF_LOG_INFO("角色: gateway, controller, 或 gateway,controller");
    MDF_LOG_INFO("预览命令: --binance-feed-spec-preview");
    const auto feed_usage = feed_kind_usage(false);
    const auto normalized_feed_usage = feed_kind_usage(true);
    MDF_LOG_INFO(
        "实时行情: --binance-live "
        "[--symbol=BTCUSDT | --symbols=BTCUSDT,ETHUSDT | --symbols=ALL] "
        "[--feed={}] [--messages=250] "
        "[--log-payload[=5]] [--payload-bytes=512] "
        "[--normalize 支持={}] [--log-normalized[=5]] "
        "[--max-attempts=1 | --reconnect] "
        "[--backoff-ms=250] [--max-backoff-ms=30000] "
        "[--reconnect-completed] [--idle-timeout-ms=30000] "
        "[--log-level=info]",
        feed_usage, normalized_feed_usage);
    std::string args_line{"参数:"};
    for (int i = 1; i < argc; ++i) {
        args_line += ' ';
        args_line += argv[i];
    }
    MDF_LOG_INFO("{}", args_line);
    return 0;
}

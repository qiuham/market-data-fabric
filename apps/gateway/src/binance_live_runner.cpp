#include "binance_live_runner.hpp"

#include "binance_live_options.hpp"
#include "binance_mapper_handlers.hpp"

#include "marketdata/providers/crypto/binance/websocket_feed_client.hpp"
#include "marketdata/runtime/logging.hpp"
#include "marketdata/service/feed_message_handler.hpp"
#include "marketdata/service/feed_payload_log_handler.hpp"
#include "marketdata/service/feed_session.hpp"
#include "marketdata/service/mapper_stats.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <ctime>
#include <string_view>

namespace md::apps::md_node {
namespace {

namespace binance = md::providers::binance;

std::atomic_bool g_stop_requested{false};

[[nodiscard]] std::string_view
feed_key_label(const md::feed::FeedConnectionSpec &connection) noexcept {
  if (connection.feeds.empty()) {
    return "";
  }
  if (connection.feeds.size() != 1) {
    return "multiple";
  }
  return connection.feeds.front().provider_feed_key;
}

void request_stop(int) noexcept {
  g_stop_requested.store(true, std::memory_order_relaxed);
}

[[nodiscard]] md::service::FeedRunStatus to_feed_run_status(
    md::net::WebSocketRunStatus status) noexcept {
  switch (status) {
  case md::net::WebSocketRunStatus::Completed:
    return md::service::FeedRunStatus::Completed;
  case md::net::WebSocketRunStatus::StoppedByHandler:
    return md::service::FeedRunStatus::StoppedByHandler;
  case md::net::WebSocketRunStatus::InvalidEndpoint:
    return md::service::FeedRunStatus::InvalidEndpoint;
  case md::net::WebSocketRunStatus::ResolveFailed:
    return md::service::FeedRunStatus::ResolveFailed;
  case md::net::WebSocketRunStatus::ConnectFailed:
    return md::service::FeedRunStatus::ConnectFailed;
  case md::net::WebSocketRunStatus::TlsHandshakeFailed:
    return md::service::FeedRunStatus::TlsHandshakeFailed;
  case md::net::WebSocketRunStatus::WebSocketHandshakeFailed:
    return md::service::FeedRunStatus::WebSocketHandshakeFailed;
  case md::net::WebSocketRunStatus::ReadFailed:
    return md::service::FeedRunStatus::ReadFailed;
  case md::net::WebSocketRunStatus::CloseFailed:
    return md::service::FeedRunStatus::CloseFailed;
  case md::net::WebSocketRunStatus::BackendUnavailable:
    return md::service::FeedRunStatus::BackendUnavailable;
  }
  return md::service::FeedRunStatus::UnknownError;
}

[[nodiscard]] md::service::FeedRunAttemptResult to_feed_attempt_result(
    const binance::BinanceWebSocketFeedClientRunResult &run_result) {
  md::service::FeedRunAttemptResult attempt{};
  attempt.status = to_feed_run_status(run_result.websocket.status);
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

[[nodiscard]] int run_binance_feed_spec_preview() {
  binance::BinanceFeedSpec spec{};
  spec.market = md::feed::MarketSegment::Spot;
  spec.symbols = {"BTCUSDT"};
  spec.kind = md::feed::FeedKind::Trade;
  const auto connection = binance::make_connection_spec(
      1, binance::BinanceEnvironment::MarketDataOnly, spec);
  const auto &feed = connection.feeds.front();
  MDF_LOG_INFO("Binance feed 配置预览");
  MDF_LOG_INFO("provider_feed_key={}", feed.provider_feed_key);
  MDF_LOG_INFO("endpoint={}", connection.endpoint);
  MDF_LOG_INFO("feed_id={}", feed.feed_id);
  return 0;
}

void log_feed_start(const md::feed::FeedConnectionSpec &connection,
                    const md::service::FeedSessionOptions &session_options,
                    const binance::BinanceWebSocketFeedClientOptions &options,
                    const BinanceLiveArgs &args) {
  MDF_LOG_INFO("行情启动 venue=binance feeds={} key={} "
               "max_messages={} normalize={} payload_log={} normalized_log={}",
               connection.feeds.size(), feed_key_label(connection),
               session_options.max_messages, args.normalize ? "true" : "false",
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
      options.websocket.idle_timeout.count(), args.payload_preview_bytes);
  for (const auto &feed : connection.feeds) {
    MDF_LOG_DEBUG("feed 已解析 key={} feed_id={}", feed.provider_feed_key,
                  feed.feed_id);
  }
}

void log_feed_stop(const md::service::FeedSessionResult &result,
                   const BinanceLiveArgs &args,
                   const md::service::FeedPayloadLogHandler &payload_log_handler,
                   const md::service::MapperRuntimeStats &mapper_stats,
                   double active_sec, double active_msg_per_sec,
                   double cpu_sec, double cpu_us_per_msg, double avg_bytes) {
  if (args.log_payload || args.normalize) {
    MDF_LOG_INFO("行情停止 reason={} status={} messages={} bytes={} "
                 "payloads={} mapped={} map_failures={}",
                 md::service::feed_session_stop_reason_name(result.stop_reason),
                 md::service::feed_run_status_name(result.stats.last_status),
                 result.stats.messages, result.stats.bytes,
                 args.log_payload ? payload_log_handler.logged() : 0,
                 args.normalize ? mapper_stats.output_events : 0,
                 args.normalize ? mapper_stats.failures : 0);
  } else {
    MDF_LOG_INFO("行情停止 reason={} status={} messages={} bytes={}",
                 md::service::feed_session_stop_reason_name(result.stop_reason),
                 md::service::feed_run_status_name(result.stats.last_status),
                 result.stats.messages, result.stats.bytes);
  }
  if (!result.stats.last_error.empty()) {
    MDF_LOG_WARN("最后错误 last_error={}", result.stats.last_error);
  }
  MDF_LOG_DEBUG("行情统计 attempts={} reconnects={} error_class={} "
                "active_sec={} active_msg_per_sec={} cpu_sec={} "
                "cpu_us_per_msg={} avg_bytes={}",
                result.stats.attempts, result.stats.reconnects,
                md::service::feed_error_class_name(result.stats.last_error_class),
                active_sec, active_msg_per_sec, cpu_sec, cpu_us_per_msg,
                avg_bytes);
}

[[nodiscard]] int run_binance_live(int argc, char **argv) {
  const auto args = parse_binance_live_args(argc, argv);
  binance::BinanceFeedSpec spec{};
  spec.market = md::feed::MarketSegment::Spot;
  spec.symbols = args.symbols;
  spec.kind = args.feed_kind;
  if (spec.kind == md::feed::FeedKind::BookSnapshot) {
    spec.depth = md::feed::FeedDepth::Top20;
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
  session_options.retry.reconnect_on_completed = args.reconnect_on_completed;

  log_feed_start(connection, session_options, options, args);

  md::service::FeedPayloadLogOptions payload_log_options{};
  payload_log_options.max_payloads = args.log_payloads;
  payload_log_options.max_payload_bytes = args.payload_preview_bytes;
  payload_log_options.log_metadata = args.log_payload;
  payload_log_options.log_payload = args.log_payload;
  md::service::FeedPayloadLogHandler payload_log_handler{payload_log_options};

  binance::BinanceMappingContext mapping_context{};
  md::service::MapperRuntimeStats mapper_stats{};
  BinanceQuoteMapperHandler quote_mapper_handler{};
  BinanceTradeMapperHandler trade_mapper_handler{};
  if (args.normalize) {
    if (connection.feeds.size() != 1 ||
        connection.feeds.front().provider_symbol.empty()) {
      MDF_LOG_WARN("标准化当前要求只订阅一个 Binance symbol feed");
    } else {
      const auto &feed = connection.feeds.front();
      const auto instrument_id =
          make_binance_spot_instrument_id(feed.provider_symbol);
      mapping_context =
          binance::make_binance_mapping_context(feed.provider_symbol,
                                                instrument_id);
      if (spec.kind == md::feed::FeedKind::TopOfBook) {
        quote_mapper_handler.context = &mapping_context;
        quote_mapper_handler.stats = &mapper_stats;
        quote_mapper_handler.max_logged_events = args.log_normalized_events;
        quote_mapper_handler.log_events = args.log_normalized;
      } else if (spec.kind == md::feed::FeedKind::Trade) {
        trade_mapper_handler.context = &mapping_context;
        trade_mapper_handler.stats = &mapper_stats;
        trade_mapper_handler.max_logged_events = args.log_normalized_events;
        trade_mapper_handler.log_events = args.log_normalized;
      } else {
        MDF_LOG_WARN("标准化当前只支持 Binance bookTicker 和 trade");
      }
    }
  }

  md::service::FeedMessagePipeline<4> handler_pipeline{};
  if (args.log_payload) {
    (void)handler_pipeline.add(&md::service::FeedPayloadLogHandler::handle,
                               &payload_log_handler);
  }
  if (quote_mapper_handler.context != nullptr) {
    (void)handler_pipeline.add(&BinanceQuoteMapperHandler::handle,
                               &quote_mapper_handler);
  }
  if (trade_mapper_handler.context != nullptr) {
    (void)handler_pipeline.add(&BinanceTradeMapperHandler::handle,
                               &trade_mapper_handler);
  }

  md::feed::FeedMessageHandler handler = nullptr;
  void *handler_data = nullptr;
  if (!handler_pipeline.empty()) {
    handler = &md::service::FeedMessagePipeline<4>::handle;
    handler_data = &handler_pipeline;
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
        binance::BinanceWebSocketFeedClient client{connection, attempt_options};
        return to_feed_attempt_result(
            client.run(attempt_context.handler, attempt_context.user_data));
      },
      handler, handler_data, stop_token);
  const auto cpu_ended = std::clock();

  const auto active_ns =
      result.stats.last_recv_ts_ns > result.stats.first_recv_ts_ns
          ? result.stats.last_recv_ts_ns - result.stats.first_recv_ts_ns
          : 0;
  const double active_sec = static_cast<double>(active_ns) / 1'000'000'000.0;
  const double active_msg_per_sec =
      active_sec > 0.0 && result.stats.messages > 1
          ? static_cast<double>(result.stats.messages - 1) / active_sec
          : 0.0;
  const double avg_bytes =
      result.stats.messages > 0
          ? static_cast<double>(result.stats.bytes) /
                static_cast<double>(result.stats.messages)
          : 0.0;
  const double cpu_sec = static_cast<double>(cpu_ended - cpu_started) /
                         static_cast<double>(CLOCKS_PER_SEC);
  const double cpu_us_per_msg =
      result.stats.messages > 0
          ? (cpu_sec * 1'000'000.0) / static_cast<double>(result.stats.messages)
          : 0.0;

  log_feed_stop(result, args, payload_log_handler, mapper_stats, active_sec,
                active_msg_per_sec, cpu_sec, cpu_us_per_msg, avg_bytes);
  return result.ok() ? 0 : 1;
}

} // 匿名命名空间

bool is_binance_command(std::string_view command) noexcept {
  return command == "--binance-feed-spec-preview" || command == "--binance-live" ||
         command == "--binance-live-preview";
}

int run_binance_command(std::string_view command, int argc, char **argv) {
  if (command == "--binance-feed-spec-preview") {
    return run_binance_feed_spec_preview();
  }
  if (command == "--binance-live" || command == "--binance-live-preview") {
    return run_binance_live(argc, argv);
  }
  return 1;
}

void log_binance_usage() {
  const auto feed_usage = binance_feed_kind_usage(false);
  const auto normalized_feed_usage = binance_feed_kind_usage(true);
  MDF_LOG_INFO("预览命令: --binance-feed-spec-preview");
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
}

} // 命名空间 md::apps::md_node

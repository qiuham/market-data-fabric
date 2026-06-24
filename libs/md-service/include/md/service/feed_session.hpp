#pragma once

#include "md/core/feed.hpp"
#include "md/net/websocket.hpp"
#include "md/service/feed_message_handler.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

namespace md::service {

enum class FeedRunStatus : std::uint8_t {
  Completed = 0,
  StoppedByHandler = 1,
  InvalidEndpoint = 2,
  ResolveFailed = 3,
  ConnectFailed = 4,
  TlsHandshakeFailed = 5,
  WebSocketHandshakeFailed = 6,
  ReadFailed = 7,
  CloseFailed = 8,
  BackendUnavailable = 9,
  UnknownError = 10,
};

enum class FeedRunErrorClass : std::uint8_t {
  None = 0,
  Stopped = 1,
  Configuration = 2,
  Dependency = 3,
  Network = 4,
  Protocol = 5,
  Io = 6,
  Unknown = 7,
};

enum class FeedSessionStopReason : std::uint8_t {
  Completed = 0,
  MaxMessagesReached = 1,
  StoppedByHandler = 2,
  StopRequested = 3,
  MaxAttemptsReached = 4,
  NotRetryable = 5,
};

struct FeedRunAttemptContext {
  std::uint64_t max_messages{};
  md::core::FeedMessageHandler handler{};
  void *user_data{};
};

struct FeedRunAttemptResult {
  FeedRunStatus status{FeedRunStatus::UnknownError};
  FeedRunErrorClass error_class{FeedRunErrorClass::Unknown};
  std::uint64_t messages{};
  std::uint64_t bytes{};
  std::uint64_t started_ns{};
  std::uint64_t ended_ns{};
  std::uint64_t first_recv_ts_ns{};
  std::uint64_t last_recv_ts_ns{};
  std::string error{};
};

struct FeedRetryPolicy {
  // 0 means unlimited attempts. Keep default at 1 for CLI/debug commands.
  std::uint32_t max_attempts{1};
  std::chrono::milliseconds initial_backoff{std::chrono::milliseconds{250}};
  std::chrono::milliseconds max_backoff{std::chrono::seconds{30}};
  double multiplier{2.0};
  bool reconnect_on_completed{false};
};

struct FeedSessionOptions {
  std::uint64_t max_messages{};
  FeedRetryPolicy retry{};
};

struct FeedStopToken {
  const std::atomic_bool *requested{};

  [[nodiscard]] bool stop_requested() const noexcept {
    return requested != nullptr && requested->load(std::memory_order_relaxed);
  }
};

struct FeedSessionStats {
  std::uint64_t attempts{};
  std::uint64_t reconnects{};
  std::uint64_t messages{};
  std::uint64_t bytes{};
  std::uint64_t first_recv_ts_ns{};
  std::uint64_t last_recv_ts_ns{};
  FeedRunStatus last_status{FeedRunStatus::UnknownError};
  FeedRunErrorClass last_error_class{FeedRunErrorClass::Unknown};
  std::string last_error{};

  void observe(const md::core::FeedMessageView &message) noexcept {
    if (messages == 0) {
      first_recv_ts_ns = message.envelope.recv_ts_ns;
    }
    ++messages;
    bytes += static_cast<std::uint64_t>(message.payload.size());
    last_recv_ts_ns = message.envelope.recv_ts_ns;
  }
};

struct FeedSessionResult {
  FeedSessionStats stats{};
  FeedSessionStopReason stop_reason{FeedSessionStopReason::Completed};

  [[nodiscard]] bool ok() const noexcept {
    return stop_reason == FeedSessionStopReason::Completed ||
           stop_reason == FeedSessionStopReason::MaxMessagesReached ||
           stop_reason == FeedSessionStopReason::StoppedByHandler ||
           stop_reason == FeedSessionStopReason::StopRequested;
  }
};

inline FeedRunStatus
to_feed_run_status(md::net::WebSocketRunStatus status) noexcept {
  switch (status) {
  case md::net::WebSocketRunStatus::Completed:
    return FeedRunStatus::Completed;
  case md::net::WebSocketRunStatus::StoppedByHandler:
    return FeedRunStatus::StoppedByHandler;
  case md::net::WebSocketRunStatus::InvalidEndpoint:
    return FeedRunStatus::InvalidEndpoint;
  case md::net::WebSocketRunStatus::ResolveFailed:
    return FeedRunStatus::ResolveFailed;
  case md::net::WebSocketRunStatus::ConnectFailed:
    return FeedRunStatus::ConnectFailed;
  case md::net::WebSocketRunStatus::TlsHandshakeFailed:
    return FeedRunStatus::TlsHandshakeFailed;
  case md::net::WebSocketRunStatus::WebSocketHandshakeFailed:
    return FeedRunStatus::WebSocketHandshakeFailed;
  case md::net::WebSocketRunStatus::ReadFailed:
    return FeedRunStatus::ReadFailed;
  case md::net::WebSocketRunStatus::CloseFailed:
    return FeedRunStatus::CloseFailed;
  case md::net::WebSocketRunStatus::BackendUnavailable:
    return FeedRunStatus::BackendUnavailable;
  }
  return FeedRunStatus::UnknownError;
}

inline FeedRunErrorClass classify_feed_run_status(FeedRunStatus status) noexcept {
  switch (status) {
  case FeedRunStatus::Completed:
    return FeedRunErrorClass::None;
  case FeedRunStatus::StoppedByHandler:
    return FeedRunErrorClass::Stopped;
  case FeedRunStatus::InvalidEndpoint:
    return FeedRunErrorClass::Configuration;
  case FeedRunStatus::BackendUnavailable:
    return FeedRunErrorClass::Dependency;
  case FeedRunStatus::ResolveFailed:
  case FeedRunStatus::ConnectFailed:
  case FeedRunStatus::TlsHandshakeFailed:
    return FeedRunErrorClass::Network;
  case FeedRunStatus::WebSocketHandshakeFailed:
    return FeedRunErrorClass::Protocol;
  case FeedRunStatus::ReadFailed:
  case FeedRunStatus::CloseFailed:
    return FeedRunErrorClass::Io;
  case FeedRunStatus::UnknownError:
    return FeedRunErrorClass::Unknown;
  }
  return FeedRunErrorClass::Unknown;
}

inline bool is_retryable_feed_error(FeedRunErrorClass error_class) noexcept {
  return error_class == FeedRunErrorClass::Network ||
         error_class == FeedRunErrorClass::Protocol ||
         error_class == FeedRunErrorClass::Io ||
         error_class == FeedRunErrorClass::Unknown;
}

inline const char *feed_run_status_name(FeedRunStatus status) noexcept {
  switch (status) {
  case FeedRunStatus::Completed:
    return "completed";
  case FeedRunStatus::StoppedByHandler:
    return "stopped_by_handler";
  case FeedRunStatus::InvalidEndpoint:
    return "invalid_endpoint";
  case FeedRunStatus::ResolveFailed:
    return "resolve_failed";
  case FeedRunStatus::ConnectFailed:
    return "connect_failed";
  case FeedRunStatus::TlsHandshakeFailed:
    return "tls_handshake_failed";
  case FeedRunStatus::WebSocketHandshakeFailed:
    return "websocket_handshake_failed";
  case FeedRunStatus::ReadFailed:
    return "read_failed";
  case FeedRunStatus::CloseFailed:
    return "close_failed";
  case FeedRunStatus::BackendUnavailable:
    return "backend_unavailable";
  case FeedRunStatus::UnknownError:
    return "unknown_error";
  }
  return "unknown_error";
}

inline const char *
feed_error_class_name(FeedRunErrorClass error_class) noexcept {
  switch (error_class) {
  case FeedRunErrorClass::None:
    return "none";
  case FeedRunErrorClass::Stopped:
    return "stopped";
  case FeedRunErrorClass::Configuration:
    return "configuration";
  case FeedRunErrorClass::Dependency:
    return "dependency";
  case FeedRunErrorClass::Network:
    return "network";
  case FeedRunErrorClass::Protocol:
    return "protocol";
  case FeedRunErrorClass::Io:
    return "io";
  case FeedRunErrorClass::Unknown:
    return "unknown";
  }
  return "unknown";
}

inline const char *
feed_session_stop_reason_name(FeedSessionStopReason reason) noexcept {
  switch (reason) {
  case FeedSessionStopReason::Completed:
    return "completed";
  case FeedSessionStopReason::MaxMessagesReached:
    return "max_messages_reached";
  case FeedSessionStopReason::StoppedByHandler:
    return "stopped_by_handler";
  case FeedSessionStopReason::StopRequested:
    return "stop_requested";
  case FeedSessionStopReason::MaxAttemptsReached:
    return "max_attempts_reached";
  case FeedSessionStopReason::NotRetryable:
    return "not_retryable";
  }
  return "unknown";
}

inline std::chrono::milliseconds next_backoff(
    std::chrono::milliseconds current, const FeedRetryPolicy &policy) noexcept {
  if (current.count() <= 0) {
    return policy.initial_backoff;
  }
  const auto next_count = static_cast<long long>(
      static_cast<double>(current.count()) * policy.multiplier);
  return std::min(std::chrono::milliseconds{next_count}, policy.max_backoff);
}

inline bool sleep_for_backoff(std::chrono::milliseconds duration,
                              FeedStopToken stop_token) {
  auto remaining = duration;
  constexpr auto kStopPollInterval = std::chrono::milliseconds{100};
  while (remaining.count() > 0) {
    if (stop_token.stop_requested()) {
      return false;
    }
    const auto chunk = std::min(remaining, kStopPollInterval);
    std::this_thread::sleep_for(chunk);
    remaining -= chunk;
  }
  return !stop_token.stop_requested();
}

namespace detail {

struct FeedSessionCallbackState {
  const FeedSessionOptions *options{};
  FeedSessionStats *stats{};
  FeedStopToken stop_token{};
  md::core::FeedMessageHandler downstream_handler{};
  void *downstream_user_data{};
  bool stopped_by_handler{};
  bool stop_requested{};
  bool max_messages_reached{};
};

inline bool on_feed_session_message(const md::core::FeedMessageView &message,
                                    void *user_data) noexcept {
  auto *state = static_cast<FeedSessionCallbackState *>(user_data);
  if (state == nullptr || state->stats == nullptr || state->options == nullptr) {
    return false;
  }

  state->stats->observe(message);

  if (state->stop_token.stop_requested()) {
    state->stop_requested = true;
    return false;
  }

  if (state->downstream_handler != nullptr &&
      !state->downstream_handler(message, state->downstream_user_data)) {
    state->stopped_by_handler = true;
    return false;
  }

  if (state->options->max_messages != 0 &&
      state->stats->messages >= state->options->max_messages) {
    state->max_messages_reached = true;
    return false;
  }

  return true;
}

} // namespace detail

template <typename RunOnce>
FeedSessionResult run_feed_session(const FeedSessionOptions &options,
                                   RunOnce &&run_once,
                                   md::core::FeedMessageHandler handler,
                                   void *user_data,
                                   FeedStopToken stop_token = {}) {
  FeedSessionResult session{};
  auto backoff = std::chrono::milliseconds{0};

  for (;;) {
    if (stop_token.stop_requested()) {
      session.stop_reason = FeedSessionStopReason::StopRequested;
      return session;
    }
    if (options.max_messages != 0 &&
        session.stats.messages >= options.max_messages) {
      session.stop_reason = FeedSessionStopReason::MaxMessagesReached;
      return session;
    }
    if (options.retry.max_attempts != 0 &&
        session.stats.attempts >= options.retry.max_attempts) {
      session.stop_reason = FeedSessionStopReason::MaxAttemptsReached;
      return session;
    }

    const auto remaining_messages =
        options.max_messages == 0 ? 0 : options.max_messages - session.stats.messages;

    detail::FeedSessionCallbackState callback_state{};
    callback_state.options = &options;
    callback_state.stats = &session.stats;
    callback_state.stop_token = stop_token;
    callback_state.downstream_handler = handler;
    callback_state.downstream_user_data = user_data;

    FeedRunAttemptContext attempt_context{};
    attempt_context.max_messages = remaining_messages;
    attempt_context.handler = &detail::on_feed_session_message;
    attempt_context.user_data = &callback_state;

    ++session.stats.attempts;
    auto attempt = run_once(attempt_context);
    session.stats.last_status = attempt.status;
    session.stats.last_error_class = attempt.error_class;
    session.stats.last_error = attempt.error;

    if (callback_state.max_messages_reached) {
      session.stop_reason = FeedSessionStopReason::MaxMessagesReached;
      return session;
    }
    if (callback_state.stop_requested || stop_token.stop_requested()) {
      session.stop_reason = FeedSessionStopReason::StopRequested;
      return session;
    }
    if (callback_state.stopped_by_handler) {
      session.stop_reason = FeedSessionStopReason::StoppedByHandler;
      return session;
    }

    if (attempt.status == FeedRunStatus::Completed &&
        !options.retry.reconnect_on_completed) {
      session.stop_reason = FeedSessionStopReason::Completed;
      return session;
    }

    if (!is_retryable_feed_error(attempt.error_class) &&
        attempt.status != FeedRunStatus::Completed) {
      session.stop_reason = FeedSessionStopReason::NotRetryable;
      return session;
    }

    if (options.retry.max_attempts != 0 &&
        session.stats.attempts >= options.retry.max_attempts) {
      session.stop_reason = FeedSessionStopReason::MaxAttemptsReached;
      return session;
    }

    ++session.stats.reconnects;
    backoff = next_backoff(backoff, options.retry);
    if (!sleep_for_backoff(backoff, stop_token)) {
      session.stop_reason = FeedSessionStopReason::StopRequested;
      return session;
    }
  }
}

} // namespace md::service

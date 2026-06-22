#include "md/service/feed_session.hpp"

#include <cassert>
#include <chrono>
#include <string_view>

namespace {

md::core::FeedMessageView make_message(std::uint64_t seq,
                                       std::uint64_t recv_ts_ns,
                                       std::string_view payload = "{}") {
  md::core::FeedMessageView message{};
  message.envelope.payload_kind = md::core::PayloadKind::ProviderMessage;
  message.envelope.payload_encoding = md::core::PayloadEncoding::ProviderJson;
  message.envelope.capture_seq = seq;
  message.envelope.recv_ts_ns = recv_ts_ns;
  message.envelope.payload_size = static_cast<std::uint32_t>(payload.size());
  message.payload = payload;
  return message;
}

md::service::FeedRunAttemptResult attempt_result(
    md::service::FeedRunStatus status) {
  md::service::FeedRunAttemptResult result{};
  result.status = status;
  result.error_class = md::service::classify_feed_run_status(status);
  return result;
}

} // namespace

int main() {
  {
    md::service::FeedSessionOptions options{};
    options.max_messages = 2;
    options.retry.max_attempts = 3;
    options.retry.initial_backoff = std::chrono::milliseconds{0};

    std::uint32_t calls = 0;
    const auto result = md::service::run_feed_session(
        options,
        [&](const md::service::FeedRunAttemptContext &context) {
          ++calls;
          if (calls == 1) {
            return attempt_result(md::service::FeedRunStatus::ConnectFailed);
          }

          const auto now = md::net::steady_now_ns();
          assert(context.handler(make_message(1, now), context.user_data));
          assert(!context.handler(make_message(2, now + 1), context.user_data));
          return attempt_result(md::service::FeedRunStatus::StoppedByHandler);
        },
        nullptr, nullptr);

    assert(result.ok());
    assert(result.stop_reason ==
           md::service::FeedSessionStopReason::MaxMessagesReached);
    assert(result.stats.attempts == 2);
    assert(result.stats.reconnects == 1);
    assert(result.stats.messages == 2);
    assert(result.stats.bytes == 4);
  }

  {
    md::service::FeedSessionOptions options{};
    options.retry.max_attempts = 3;
    options.retry.initial_backoff = std::chrono::milliseconds{0};

    const auto result = md::service::run_feed_session(
        options,
        [](const md::service::FeedRunAttemptContext &) {
          return attempt_result(
              md::service::FeedRunStatus::BackendUnavailable);
        },
        nullptr, nullptr);

    assert(!result.ok());
    assert(result.stop_reason ==
           md::service::FeedSessionStopReason::NotRetryable);
    assert(result.stats.attempts == 1);
    assert(result.stats.reconnects == 0);
    assert(result.stats.last_error_class ==
           md::service::FeedRunErrorClass::Dependency);
  }

  {
    md::service::FeedSessionOptions options{};
    options.retry.max_attempts = 2;
    options.retry.initial_backoff = std::chrono::milliseconds{0};

    std::uint32_t calls = 0;
    const auto result = md::service::run_feed_session(
        options,
        [&](const md::service::FeedRunAttemptContext &) {
          ++calls;
          return attempt_result(md::service::FeedRunStatus::ReadFailed);
        },
        nullptr, nullptr);

    assert(!result.ok());
    assert(result.stop_reason ==
           md::service::FeedSessionStopReason::MaxAttemptsReached);
    assert(result.stats.attempts == 2);
    assert(result.stats.reconnects == 1);
    assert(calls == 2);
  }

  {
    md::service::FeedSessionOptions options{};
    options.retry.max_attempts = 2;
    options.retry.initial_backoff = std::chrono::milliseconds{0};

    std::uint32_t calls = 0;
    const auto result = md::service::run_feed_session(
        options,
        [&](const md::service::FeedRunAttemptContext &) {
          ++calls;
          return attempt_result(md::service::FeedRunStatus::Completed);
        },
        nullptr, nullptr);

    assert(result.ok());
    assert(result.stop_reason ==
           md::service::FeedSessionStopReason::Completed);
    assert(result.stats.attempts == 1);
    assert(result.stats.reconnects == 0);
    assert(calls == 1);
  }

  return 0;
}

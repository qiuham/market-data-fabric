#include "marketdata/service/feed_session.hpp"
#include "marketdata/service/mapper_stats.hpp"

#include <cassert>
#include <chrono>
#include <string_view>

namespace {

md::feed::FeedMessageView make_message(std::uint64_t seq,
                                       std::uint64_t recv_ts_ns,
                                       std::string_view payload = "{}") {
  md::feed::FeedMessageView message{};
  message.envelope.payload_kind = md::feed::PayloadKind::RawProviderMessage;
  message.envelope.payload_encoding = md::feed::PayloadEncoding::ProviderJson;
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

} // 匿名命名空间

int main() {
  {
    md::service::MapperRuntimeStats stats{};
    const auto first = make_message(1, 1000, "one");
    const auto second = make_message(2, 2000, "two");

    stats.observe_success(first);
    stats.observe_failure(second);

    assert(stats.input_messages == 2);
    assert(stats.output_events == 1);
    assert(stats.failures == 1);
    assert(stats.first_recv_ts_ns == 1000);
    assert(stats.last_recv_ts_ns == 2000);
    assert(stats.last_payload_size == 3);
  }

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

          const std::uint64_t now = 1000;
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

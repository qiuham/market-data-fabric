#pragma once

#include "md/core/feed.hpp"
#include "md/runtime/logging.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace md::service {

struct FeedMessageStats {
  std::uint64_t messages{};
  std::uint64_t bytes{};
  std::uint64_t first_recv_ts_ns{};
  std::uint64_t last_recv_ts_ns{};
  std::uint32_t last_payload_size{};

  void observe(const md::core::FeedMessageView &message) noexcept {
    if (messages == 0) {
      first_recv_ts_ns = message.envelope.recv_ts_ns;
    }
    ++messages;
    bytes += static_cast<std::uint64_t>(message.payload.size());
    last_recv_ts_ns = message.envelope.recv_ts_ns;
    last_payload_size = message.envelope.payload_size;
  }
};

struct FeedMessageHandlerBinding {
  md::core::FeedMessageHandler handler{};
  void *user_data{};
};

inline bool dispatch_feed_message(const md::core::FeedMessageView &message,
                                  void *user_data) noexcept {
  auto *binding = static_cast<FeedMessageHandlerBinding *>(user_data);
  if (binding == nullptr || binding->handler == nullptr) {
    return true;
  }
  return binding->handler(message, binding->user_data);
}

struct FeedMessageLogOptions {
  std::uint64_t max_payloads{};
  std::size_t max_payload_bytes{512};
  bool log_metadata{true};
  bool log_payload{false};
};

class FeedMessageLogHandler final {
public:
  explicit FeedMessageLogHandler(FeedMessageLogOptions options) noexcept
      : options_(options) {}

  static bool handle(const md::core::FeedMessageView &message,
                     void *user_data) noexcept {
    auto *handler = static_cast<FeedMessageLogHandler *>(user_data);
    return handler == nullptr || handler->on_message(message);
  }

  bool on_message(const md::core::FeedMessageView &message) noexcept {
    ++seen_;
    if (options_.max_payloads != 0 && logged_ >= options_.max_payloads) {
      return true;
    }
    if (!options_.log_metadata && !options_.log_payload) {
      return true;
    }

    ++logged_;
    try {
      std::string_view payload{};
      bool truncated = false;
      if (options_.log_payload) {
        payload = payload_preview(message.payload);
        truncated = payload.size() < message.payload.size();
      }

      if (options_.log_metadata) {
        if (options_.log_payload) {
          if (truncated) {
            MDF_LOG_INFO(
                "raw feed_id={} payload_size={} recv_ts_ns={} payload={}...",
                message.envelope.feed_id, message.envelope.payload_size,
                message.envelope.recv_ts_ns, payload);
          } else {
            MDF_LOG_INFO(
                "raw feed_id={} payload_size={} recv_ts_ns={} payload={}",
                message.envelope.feed_id, message.envelope.payload_size,
                message.envelope.recv_ts_ns, payload);
          }
        } else {
          MDF_LOG_INFO("raw feed_id={} payload_size={} recv_ts_ns={}",
                       message.envelope.feed_id, message.envelope.payload_size,
                       message.envelope.recv_ts_ns);
        }
      } else if (options_.log_payload) {
        if (truncated) {
          MDF_LOG_INFO("raw payload={}...", payload);
        } else {
          MDF_LOG_INFO("raw payload={}", payload);
        }
      }
    } catch (...) {
      return false;
    }
    return true;
  }

  [[nodiscard]] std::uint64_t seen() const noexcept { return seen_; }
  [[nodiscard]] std::uint64_t logged() const noexcept { return logged_; }

private:
  [[nodiscard]] std::string_view
  payload_preview(std::string_view payload) const noexcept {
    if (options_.max_payload_bytes == 0) {
      return payload;
    }
    return payload.substr(0, std::min(payload.size(), options_.max_payload_bytes));
  }

  FeedMessageLogOptions options_{};
  std::uint64_t seen_{};
  std::uint64_t logged_{};
};

} // namespace md::service

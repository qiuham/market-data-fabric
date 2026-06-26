#pragma once

#include "md/core/feed.hpp"
#include "md/runtime/logging.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace md::service {

struct FeedPayloadLogOptions {
  std::uint64_t max_payloads{};
  std::size_t max_payload_bytes{512};
  bool log_metadata{true};
  bool log_payload{false};
};

class FeedPayloadLogHandler final {
public:
  explicit FeedPayloadLogHandler(FeedPayloadLogOptions options) noexcept
      : options_(options) {}

  static bool handle(const md::core::FeedMessageView &message,
                     void *user_data) noexcept {
    auto *handler = static_cast<FeedPayloadLogHandler *>(user_data);
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
                "原始 payload feed_id={} payload_size={} recv_ts_ns={} "
                "payload={}...",
                message.envelope.feed_id, message.envelope.payload_size,
                message.envelope.recv_ts_ns, payload);
          } else {
            MDF_LOG_INFO(
                "原始 payload feed_id={} payload_size={} recv_ts_ns={} "
                "payload={}",
                message.envelope.feed_id, message.envelope.payload_size,
                message.envelope.recv_ts_ns, payload);
          }
        } else {
          MDF_LOG_INFO("原始 payload feed_id={} payload_size={} recv_ts_ns={}",
                       message.envelope.feed_id, message.envelope.payload_size,
                       message.envelope.recv_ts_ns);
        }
      } else if (options_.log_payload) {
        if (truncated) {
          MDF_LOG_INFO("原始 payload={}...", payload);
        } else {
          MDF_LOG_INFO("原始 payload={}", payload);
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

  FeedPayloadLogOptions options_{};
  std::uint64_t seen_{};
  std::uint64_t logged_{};
};

} // 命名空间 md::service

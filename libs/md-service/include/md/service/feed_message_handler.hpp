#pragma once

#include "md/core/feed.hpp"

#include <cstdint>

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

} // 命名空间 md::service

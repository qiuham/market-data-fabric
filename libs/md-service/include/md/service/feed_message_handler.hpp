#pragma once

#include "md/core/feed.hpp"

#include <array>
#include <cstddef>
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

template <std::size_t MaxHandlers = 8> class FeedMessagePipeline final {
public:
  [[nodiscard]] bool add(md::core::FeedMessageHandler handler,
                         void *user_data = nullptr) noexcept {
    if (handler == nullptr) {
      return true;
    }
    if (count_ >= MaxHandlers) {
      return false;
    }
    bindings_[count_++] = FeedMessageHandlerBinding{handler, user_data};
    return true;
  }

  [[nodiscard]] bool empty() const noexcept { return count_ == 0; }
  [[nodiscard]] std::size_t size() const noexcept { return count_; }

  bool on_message(const md::core::FeedMessageView &message) noexcept {
    for (std::size_t i = 0; i < count_; ++i) {
      const auto &binding = bindings_[i];
      if (binding.handler != nullptr &&
          !binding.handler(message, binding.user_data)) {
        return false;
      }
    }
    return true;
  }

  static bool handle(const md::core::FeedMessageView &message,
                     void *user_data) noexcept {
    auto *pipeline = static_cast<FeedMessagePipeline *>(user_data);
    return pipeline == nullptr || pipeline->on_message(message);
  }

private:
  std::array<FeedMessageHandlerBinding, MaxHandlers> bindings_{};
  std::size_t count_{};
};

} // 命名空间 md::service

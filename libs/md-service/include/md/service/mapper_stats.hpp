#pragma once

#include "md/core/feed.hpp"

#include <cstdint>

namespace md::service {

struct MapperRuntimeStats {
  std::uint64_t input_messages{};
  std::uint64_t output_events{};
  std::uint64_t failures{};
  std::uint64_t first_recv_ts_ns{};
  std::uint64_t last_recv_ts_ns{};
  std::uint32_t last_payload_size{};

  void observe_success(const md::core::FeedMessageView &message,
                       std::uint64_t events = 1) noexcept {
    observe_input(message);
    output_events += events;
  }

  void observe_failure(const md::core::FeedMessageView &message) noexcept {
    observe_input(message);
    ++failures;
  }

private:
  void observe_input(const md::core::FeedMessageView &message) noexcept {
    if (input_messages == 0) {
      first_recv_ts_ns = message.envelope.recv_ts_ns;
    }
    ++input_messages;
    last_recv_ts_ns = message.envelope.recv_ts_ns;
    last_payload_size = message.envelope.payload_size;
  }
};

} // namespace md::service

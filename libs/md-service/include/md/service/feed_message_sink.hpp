#pragma once

#include "md/core/feed.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ostream>
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

struct FeedMessageSink {
  md::core::FeedMessageHandler handler{};
  void *user_data{};
};

inline bool dispatch_feed_message(const md::core::FeedMessageView &message,
                                  void *user_data) noexcept {
  auto *sink = static_cast<FeedMessageSink *>(user_data);
  if (sink == nullptr || sink->handler == nullptr) {
    return true;
  }
  return sink->handler(message, sink->user_data);
}

struct OstreamFeedMessagePrinterOptions {
  std::uint64_t max_payloads{};
  std::size_t max_payload_bytes{512};
  bool print_metadata{true};
  bool print_payload{false};
};

class OstreamFeedMessagePrinter final {
public:
  OstreamFeedMessagePrinter(std::ostream &out,
                            OstreamFeedMessagePrinterOptions options) noexcept
      : out_(&out), options_(options) {}

  static bool handle(const md::core::FeedMessageView &message,
                     void *user_data) noexcept {
    auto *printer = static_cast<OstreamFeedMessagePrinter *>(user_data);
    return printer == nullptr || printer->on_message(message);
  }

  bool on_message(const md::core::FeedMessageView &message) noexcept {
    ++seen_;
    if (options_.max_payloads != 0 && printed_ >= options_.max_payloads) {
      return true;
    }
    if (!options_.print_metadata && !options_.print_payload) {
      return true;
    }

    ++printed_;
    try {
      if (options_.print_metadata) {
        *out_ << "message=" << seen_
              << " capture_seq=" << message.envelope.capture_seq
              << " feed_id=" << message.envelope.feed_id
              << " payload_size=" << message.envelope.payload_size
              << " recv_ts_ns=" << message.envelope.recv_ts_ns << '\n';
      }
      if (options_.print_payload) {
        const auto payload = payload_preview(message.payload);
        out_->write(payload.data(), static_cast<std::streamsize>(payload.size()));
        if (payload.size() < message.payload.size()) {
          *out_ << "...";
        }
        *out_ << '\n';
      }
    } catch (...) {
      return false;
    }
    return true;
  }

  [[nodiscard]] std::uint64_t seen() const noexcept { return seen_; }
  [[nodiscard]] std::uint64_t printed() const noexcept { return printed_; }

private:
  [[nodiscard]] std::string_view
  payload_preview(std::string_view payload) const noexcept {
    if (options_.max_payload_bytes == 0) {
      return payload;
    }
    return payload.substr(0, std::min(payload.size(), options_.max_payload_bytes));
  }

  std::ostream *out_{};
  OstreamFeedMessagePrinterOptions options_{};
  std::uint64_t seen_{};
  std::uint64_t printed_{};
};

} // namespace md::service

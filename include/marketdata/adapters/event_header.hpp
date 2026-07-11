#pragma once

#include "marketdata/feed/message.hpp"
#include "trading/core/header.hpp"

namespace md::adapters {

[[nodiscard]] inline trading::core::EventHeader make_event_header(
    const md::feed::MessageEnvelope& message,
    trading::core::EventKind kind) noexcept {
  return trading::core::EventHeader{
      .kind = kind,
      .flags = message.flags,
      .source_id = message.source_id,
      .venue_id = message.venue_id,
      .feed_id = message.feed_id,
      .connection_id = message.connection_id,
      .instrument_id = message.instrument_id,
      .capture_seq = message.capture_seq,
      .event_seq = message.event_seq,
      .exchange_seq = message.exchange_seq,
      .exchange_ts_ns = message.source_ts_ns,
      .recv_ts_ns = message.recv_ts_ns,
      .send_ts_ns = message.publish_ts_ns,
  };
}

}  // namespace md::adapters

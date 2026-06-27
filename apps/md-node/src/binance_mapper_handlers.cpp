#include "binance_mapper_handlers.hpp"

#include "md/runtime/logging.hpp"

namespace md::apps::md_node {
namespace {

namespace binance = md::adapters::crypto::binance;

[[nodiscard]] const char *aggressor_side_name(
    md::core::AggressorSide side) noexcept {
  switch (side) {
  case md::core::AggressorSide::Buy:
    return "buy";
  case md::core::AggressorSide::Sell:
    return "sell";
  case md::core::AggressorSide::None:
    return "none";
  case md::core::AggressorSide::Unknown:
    return "unknown";
  }
  return "unknown";
}

} // 匿名命名空间

std::uint32_t make_binance_spot_instrument_id(
    std::string_view provider_symbol) {
  const auto seed = "binance:spot:" + std::string(provider_symbol);
  return md::core::stable_id32(seed);
}

bool BinanceQuoteMapperHandler::handle(
    const md::core::FeedMessageView &message, void *user_data) noexcept {
  auto *handler = static_cast<BinanceQuoteMapperHandler *>(user_data);
  return handler == nullptr || handler->on_message(message);
}

bool BinanceQuoteMapperHandler::on_message(
    const md::core::FeedMessageView &message) noexcept {
  md::core::Quote quote{};
  if (context == nullptr ||
      !binance::map_book_ticker_to_quote(message, *context, quote)) {
    if (stats != nullptr) {
      stats->observe_failure(message);
    }
    if (stats != nullptr && stats->failures <= 3) {
      MDF_LOG_WARN("Quote 标准化失败 feed_id={} payload_size={} recv_ts_ns={}",
                   message.envelope.feed_id, message.envelope.payload_size,
                   message.envelope.recv_ts_ns);
    }
    return true;
  }

  if (stats != nullptr) {
    stats->observe_success(message);
  }
  if (!log_events ||
      (max_logged_events != 0 && logged_events >= max_logged_events)) {
    return true;
  }

  ++logged_events;
  MDF_LOG_INFO(
      "Quote 标准化结果 symbol={} exchange_seq={} bid_price={} "
      "bid_quantity={} ask_price={} ask_quantity={} recv_ts_ns={}",
      context->provider_symbol, quote.header.exchange_seq, quote.bid_price,
      quote.bid_quantity, quote.ask_price, quote.ask_quantity,
      quote.header.recv_ts_ns);
  return true;
}

bool BinanceTradeMapperHandler::handle(
    const md::core::FeedMessageView &message, void *user_data) noexcept {
  auto *handler = static_cast<BinanceTradeMapperHandler *>(user_data);
  return handler == nullptr || handler->on_message(message);
}

bool BinanceTradeMapperHandler::on_message(
    const md::core::FeedMessageView &message) noexcept {
  md::core::Trade trade{};
  if (context == nullptr || !binance::map_trade_to_trade(message, *context, trade)) {
    if (stats != nullptr) {
      stats->observe_failure(message);
    }
    if (stats != nullptr && stats->failures <= 3) {
      MDF_LOG_WARN("Trade 标准化失败 feed_id={} payload_size={} recv_ts_ns={}",
                   message.envelope.feed_id, message.envelope.payload_size,
                   message.envelope.recv_ts_ns);
    }
    return true;
  }

  if (stats != nullptr) {
    stats->observe_success(message);
  }
  if (!log_events ||
      (max_logged_events != 0 && logged_events >= max_logged_events)) {
    return true;
  }

  ++logged_events;
  MDF_LOG_INFO(
      "Trade 标准化结果 symbol={} trade_id={} price={} quantity={} "
      "aggressor_side={} source_ts_ns={} recv_ts_ns={}",
      context->provider_symbol, trade.trade_id, trade.price, trade.quantity,
      aggressor_side_name(trade.aggressor_side), trade.header.source_ts_ns,
      trade.header.recv_ts_ns);
  return true;
}

} // 命名空间 md::apps::md_node

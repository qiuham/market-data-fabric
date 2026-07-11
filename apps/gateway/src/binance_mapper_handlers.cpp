#include "binance_mapper_handlers.hpp"

#include "marketdata/runtime/logging.hpp"

namespace md::apps::md_node {
namespace {

namespace binance = md::adapters::binance;

[[nodiscard]] const char *aggressor_side_name(
    trading::core::AggressorSide side) noexcept {
  switch (side) {
  case trading::core::AggressorSide::Buy:
    return "buy";
  case trading::core::AggressorSide::Sell:
    return "sell";
  case trading::core::AggressorSide::None:
    return "none";
  case trading::core::AggressorSide::Unknown:
    return "unknown";
  }
  return "unknown";
}

} // 匿名命名空间

std::uint32_t make_binance_spot_instrument_id(
    std::string_view provider_symbol) {
  const auto seed = "binance:spot:" + std::string(provider_symbol);
  return md::feed::stable_id32(seed);
}

bool BinanceQuoteMapperHandler::handle(
    const md::feed::FeedMessageView &message, void *user_data) noexcept {
  auto *handler = static_cast<BinanceQuoteMapperHandler *>(user_data);
  return handler == nullptr || handler->on_message(message);
}

bool BinanceQuoteMapperHandler::on_message(
    const md::feed::FeedMessageView &message) noexcept {
  trading::events::Quote quote{};
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
  // 0明确表示不输出逐事件日志，禁止把它解释成“无限”，避免误配置把日志I/O
  // 带入行情热路径。
  if (!log_events || max_logged_events == 0 ||
      logged_events >= max_logged_events) {
    return true;
  }

  ++logged_events;
  MDF_LOG_INFO(
      "报价标准化结果 标的={} 交易所序号={} 买价={} 买量={} 卖价={} "
      "卖量={} 接收时间戳={}",
      context->provider_symbol, quote.header.exchange_seq, quote.bid_price,
      quote.bid_qty, quote.ask_price, quote.ask_qty,
      quote.header.recv_ts_ns);
  return true;
}

bool BinanceTradeMapperHandler::handle(
    const md::feed::FeedMessageView &message, void *user_data) noexcept {
  auto *handler = static_cast<BinanceTradeMapperHandler *>(user_data);
  return handler == nullptr || handler->on_message(message);
}

bool BinanceTradeMapperHandler::on_message(
    const md::feed::FeedMessageView &message) noexcept {
  trading::events::Trade trade{};
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
  if (!log_events || max_logged_events == 0 ||
      logged_events >= max_logged_events) {
    return true;
  }

  ++logged_events;
  MDF_LOG_INFO(
      "成交标准化结果 标的={} 成交编号={} 价格={} 数量={} 主动方向={} "
      "交易所时间戳={} 接收时间戳={}",
      context->provider_symbol, trade.trade_id, trade.price, trade.quantity,
      aggressor_side_name(trade.aggressor_side), trade.header.exchange_ts_ns,
      trade.header.recv_ts_ns);
  return true;
}

} // 命名空间 md::apps::md_node

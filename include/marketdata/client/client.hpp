#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "trading/events/market.hpp"
#include "trading/events/order_book.hpp"

namespace md::client {

enum class EventMask : unsigned {
    Trade = 1u << 0,
    Quote = 1u << 1,
    Book = 1u << 2,
    TradingPhase = 1u << 3,
    Order = 1u << 4,
    BookTrade = 1u << 5,
};

struct FeedRequest {
    std::string source;
    std::string symbol;
    unsigned events{};
};

struct ClientConfig {
    std::string mode{"live"};
    std::string transport{"nats"};
    std::string endpoint;
};

class MarketDataClient {
public:
    virtual ~MarketDataClient() = default;
    virtual void request_feed(const FeedRequest& request) = 0;
    virtual void on_trade(
        std::function<void(const trading::events::Trade&)> handler) = 0;
    virtual void on_order_event(
        std::function<void(const trading::events::BookOrder&)> handler) = 0;
    virtual void on_book_trade(
        std::function<void(const trading::events::BookTrade&)> handler) = 0;
    virtual void run() = 0;
};

} // 命名空间 md::client

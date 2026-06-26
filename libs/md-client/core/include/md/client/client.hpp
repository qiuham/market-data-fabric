#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "md/core/events.hpp"

namespace md::client {

enum class EventMask : unsigned {
    Trade = 1u << 0,
    Quote = 1u << 1,
    Book = 1u << 2,
    Status = 1u << 3,
    Order = 1u << 4,
    Execution = 1u << 5,
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
    virtual void on_trade(std::function<void(const md::core::Trade&)> handler) = 0;
    virtual void on_order_event(std::function<void(const md::core::OrderEvent&)> handler) = 0;
    virtual void on_execution(std::function<void(const md::core::Execution&)> handler) = 0;
    virtual void run() = 0;
};

} // 命名空间 md::client

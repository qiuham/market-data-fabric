#pragma once

#include "md/core/events.hpp"

namespace md::core {

template <typename Impl>
class MarketDataSink {
public:
    void on_trade(const Trade& event) { impl().on_trade(event); }
    void on_quote(const Quote& event) { impl().on_quote(event); }

    template <std::size_t MaxLevels>
    void on_book_update(const BookUpdate<MaxLevels>& event) {
        impl().on_book_update(event);
    }

    void on_book_delta(const BookDelta& event) { impl().on_book_delta(event); }
    void on_order_event(const OrderEvent& event) { impl().on_order_event(event); }
    void on_execution(const Execution& event) { impl().on_execution(event); }
    void on_status(const Status& event) { impl().on_status(event); }

private:
    Impl& impl() { return static_cast<Impl&>(*this); }
};

} // namespace md::core

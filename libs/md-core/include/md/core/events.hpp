#pragma once

#include <array>
#include <cstdint>

namespace md::core {

enum class EventType : std::uint16_t {
    Trade = 1,
    Quote = 2,
    BookDelta = 3,
    BookSnapshot = 4,
    Status = 5,
    Heartbeat = 6,
    OrderEvent = 7,
    Execution = 8,
};

enum class Side : std::uint8_t {
    Unknown = 0,
    Buy = 1,
    Sell = 2,
};

enum class BookAction : std::uint8_t {
    Add = 1,
    Update = 2,
    Delete = 3,
};

enum class OrderAction : std::uint8_t {
    Unknown = 0,
    Add = 1,
    Modify = 2,
    Cancel = 3,
    Delete = 4,
    Replace = 5,
};

enum class OrderType : std::uint8_t {
    Unknown = 0,
    Limit = 1,
    Market = 2,
    Best = 3,
    BestFive = 4,
    ImmediateOrCancel = 5,
    FillOrKill = 6,
};

enum class TimeInForce : std::uint8_t {
    Unknown = 0,
    Day = 1,
    GoodTillCancel = 2,
    ImmediateOrCancel = 3,
    FillOrKill = 4,
    Auction = 5,
};

enum class ExecutionType : std::uint8_t {
    Unknown = 0,
    Trade = 1,
    Cancel = 2,
    Correct = 3,
    Bust = 4,
};

enum class AggressorSide : std::uint8_t {
    Unknown = 0,
    Buy = 1,
    Sell = 2,
    None = 3,
};

struct MdHeader {
    std::uint16_t schema_version{1};
    EventType event_type{};
    std::uint32_t source_id{};
    std::uint32_t venue_id{};
    std::uint32_t instrument_id{};
    std::uint32_t stream_id{};
    std::uint64_t seq{};
    std::uint64_t exchange_seq{};
    std::uint64_t source_ts_ns{};
    std::uint64_t recv_ts_ns{};
    std::uint64_t publish_ts_ns{};
    std::uint32_t flags{};
};

struct Trade {
    MdHeader header{.event_type = EventType::Trade};
    std::int64_t price{};
    std::int64_t quantity{};
    std::uint64_t trade_id{};
    Side side{Side::Unknown};
    std::uint8_t condition{};
};

struct Quote {
    MdHeader header{.event_type = EventType::Quote};
    std::int64_t bid_price{};
    std::int64_t bid_quantity{};
    std::int64_t ask_price{};
    std::int64_t ask_quantity{};
};

struct BookLevel {
    std::int64_t price{};
    std::int64_t quantity{};
    std::uint32_t order_count{};
};

struct BookDelta {
    MdHeader header{.event_type = EventType::BookDelta};
    Side side{Side::Unknown};
    BookAction action{BookAction::Update};
    std::uint16_t level{};
    std::int64_t price{};
    std::int64_t quantity{};
    std::uint64_t order_id{};
};

template <std::size_t Depth = 20>
struct BookSnapshot {
    MdHeader header{.event_type = EventType::BookSnapshot};
    std::array<BookLevel, Depth> bids{};
    std::array<BookLevel, Depth> asks{};
    std::uint8_t bid_count{};
    std::uint8_t ask_count{};
};

struct OrderEvent {
    MdHeader header{.event_type = EventType::OrderEvent};
    std::uint64_t order_id{};
    std::uint64_t original_order_id{};
    std::uint64_t channel_id{};
    std::uint64_t order_seq{};
    std::uint64_t priority_id{};
    std::int64_t price{};
    std::int64_t quantity{};
    std::int64_t remaining_quantity{};
    std::int64_t display_quantity{};
    Side side{Side::Unknown};
    OrderAction action{OrderAction::Unknown};
    OrderType order_type{OrderType::Unknown};
    TimeInForce time_in_force{TimeInForce::Unknown};
    std::uint16_t condition{};
    std::uint16_t trading_phase{};
};

struct Execution {
    MdHeader header{.event_type = EventType::Execution};
    std::uint64_t trade_id{};
    std::uint64_t channel_id{};
    std::uint64_t execution_seq{};
    std::uint64_t buy_order_id{};
    std::uint64_t sell_order_id{};
    std::uint64_t resting_order_id{};
    std::uint64_t aggressor_order_id{};
    std::int64_t price{};
    std::int64_t quantity{};
    ExecutionType execution_type{ExecutionType::Unknown};
    AggressorSide aggressor_side{AggressorSide::Unknown};
    std::uint16_t condition{};
    std::uint16_t trading_phase{};
};

struct Status {
    MdHeader header{.event_type = EventType::Status};
    std::uint16_t status_type{};
    std::uint16_t trading_status{};
    std::uint32_t reason_code{};
};

} // namespace md::core

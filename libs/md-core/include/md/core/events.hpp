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

struct Status {
    MdHeader header{.event_type = EventType::Status};
    std::uint16_t status_type{};
    std::uint16_t trading_status{};
    std::uint32_t reason_code{};
};

} // namespace md::core

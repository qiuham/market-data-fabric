#pragma once

#include <cstdint>

#include "md/core/events.hpp"

namespace md::book {

struct MboOrder {
    std::uint64_t order_id{};
    std::uint64_t priority_id{};
    std::int64_t price{};
    std::int64_t quantity{};
    md::core::Side side{md::core::Side::Unknown};
};

struct MboApplyResult {
    bool applied{};
    bool book_changed{};
    bool gap_detected{};
};

class MboBook {
public:
    MboApplyResult apply_order_event(const md::core::OrderEvent& event) noexcept;
    MboApplyResult apply_execution(const md::core::Execution& execution) noexcept;
    void clear() noexcept;
};

inline MboApplyResult MboBook::apply_order_event(const md::core::OrderEvent&) noexcept {
    return {.applied = false, .book_changed = false, .gap_detected = false};
}

inline MboApplyResult MboBook::apply_execution(const md::core::Execution&) noexcept {
    return {.applied = false, .book_changed = false, .gap_detected = false};
}

inline void MboBook::clear() noexcept {}

} // 命名空间 md::book

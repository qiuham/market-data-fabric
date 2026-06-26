#pragma once

#include <cstdint>
#include <string_view>

namespace md::runtime {

enum class ThreadRole : std::uint8_t {
    FeedIo,
    Decoder,
    BookBuilder,
    Publisher,
    Recorder,
    ControlPlane,
    Metrics,
};

enum class SchedulerPolicy : std::uint8_t {
    Default,
    Fifo,
    RoundRobin,
};

struct ThreadProfile {
    ThreadRole role{};
    std::string_view name{};
    std::uint16_t cpu{};
    SchedulerPolicy scheduler{SchedulerPolicy::Default};
    std::int16_t priority{};
};

} // 命名空间 md::runtime

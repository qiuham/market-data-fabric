#pragma once

#include <cstdint>

namespace md::runtime {

enum class ClockSource : std::uint8_t {
    MonotonicRaw,
    Tsc,
    Phc,
    Realtime,
};

struct ClockConfig {
    ClockSource source{ClockSource::MonotonicRaw};
    bool calibrate_tsc{false};
    bool require_ptp_sync{false};
};

class ClockReader {
public:
    virtual ~ClockReader() = default;
    virtual std::uint64_t now_ns() const noexcept = 0;
};

} // 命名空间 md::runtime

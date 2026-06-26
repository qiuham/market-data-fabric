#pragma once

#include <cstdint>

namespace md::runtime {

struct CpuSetConfig {
    const std::uint16_t* cpus{};
    std::uint16_t count{};
};

enum class AffinityResult : std::uint8_t {
    Applied,
    Unsupported,
    InvalidConfig,
    SystemError,
};

// 实现应按操作系统区分。Linux 可以使用 pthread_setaffinity_np；
// 非 Linux 开发构建可以返回 Unsupported，不影响 md-core。
AffinityResult pin_current_thread(const CpuSetConfig& config) noexcept;

} // 命名空间 md::runtime

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

// Implementations should be OS-specific. Linux can use pthread_setaffinity_np;
// Non-Linux development builds can return Unsupported without affecting md-core.
AffinityResult pin_current_thread(const CpuSetConfig& config) noexcept;

} // namespace md::runtime

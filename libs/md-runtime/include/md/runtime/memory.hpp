#pragma once

#include <cstddef>
#include <cstdint>

namespace md::runtime {

struct MemoryProfile {
    bool preallocate{true};
    bool lock_pages{false};
    bool prefer_huge_pages{false};
    std::size_t ring_bytes{};
    std::size_t arena_bytes{};
};

enum class MemoryResult : std::uint8_t {
    Applied,
    PartiallyApplied,
    Unsupported,
    SystemError,
};

MemoryResult apply_memory_profile(const MemoryProfile& profile) noexcept;

} // namespace md::runtime

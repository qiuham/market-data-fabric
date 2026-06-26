#pragma once

#include <cstdint>

namespace md::book {

enum class BookModel : std::uint8_t {
    Unknown = 0,
    L1 = 1,
    L2Mbp = 2,
    L3Mbo = 3,
};

} // 命名空间 md::book

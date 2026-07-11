#pragma once

#include <cstdint>
#include <string_view>

namespace md::adapters {

struct InstrumentMappingContext {
  std::uint32_t source_id{};
  std::uint32_t venue_id{};
  std::uint32_t instrument_id{};
  std::int32_t price_scale{};
  std::int32_t quantity_scale{};
  std::string_view provider_symbol{};
};

} // 命名空间 md::adapters

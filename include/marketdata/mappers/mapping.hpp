#pragma once

#include <cstdint>
#include <string_view>

namespace md::mappers {

// provider decoder完成字段解析后，将稳定ID和精度上下文传给组合mapper。
// provider_symbol仅在接入边界使用，不能传播到trading-core事件。
struct InstrumentMappingContext {
  std::uint32_t source_id{};
  std::uint32_t venue_id{};
  std::uint32_t instrument_id{};
  std::int32_t price_scale{};
  std::int32_t quantity_scale{};
  std::string_view provider_symbol{};
};

}  // namespace md::mappers

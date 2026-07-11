#pragma once

#include "marketdata/providers/crypto/binance/types.hpp"
#include "marketdata/mappers/mapping.hpp"

#include <cstdint>
#include <string_view>

namespace md::providers::binance {

struct BinanceMappingContext : md::mappers::InstrumentMappingContext {
  BinanceMappingContext() noexcept {
    source_id = kBinanceSourceId;
    venue_id = static_cast<std::uint32_t>(md::feed::VenueId::Binance);
    price_scale = 8;
    quantity_scale = 8;
  }
};

using BinanceBookTickerMappingContext = BinanceMappingContext;
using BinanceTradeMappingContext = BinanceMappingContext;

[[nodiscard]] inline BinanceMappingContext make_binance_mapping_context(
    std::string_view provider_symbol, std::uint32_t instrument_id,
    std::int32_t price_scale = 8,
    std::int32_t quantity_scale = 8) noexcept {
  BinanceMappingContext context{};
  context.instrument_id = instrument_id;
  context.price_scale = price_scale;
  context.quantity_scale = quantity_scale;
  context.provider_symbol = provider_symbol;
  return context;
}

} // 命名空间 md::providers::binance

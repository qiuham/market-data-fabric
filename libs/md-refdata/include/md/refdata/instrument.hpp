#pragma once

#include <cstdint>
#include <string_view>

namespace md::refdata {

enum class AssetClass : std::uint8_t {
  Unknown = 0,
  Crypto = 1,
  Equity = 2,
  Futures = 3,
  Options = 4,
  Fx = 5,
  Index = 6,
};

enum class ProductType : std::uint8_t {
  Unknown = 0,
  Spot = 1,
  Perpetual = 2,
  Future = 3,
  Option = 4,
  Index = 5,
  Fund = 6,
};

enum class OptionType : std::uint8_t {
  Unknown = 0,
  Call = 1,
  Put = 2,
};

enum class SettlementType : std::uint8_t {
  Unknown = 0,
  Physical = 1,
  Cash = 2,
};

struct Instrument {
  std::uint32_t instrument_id{};
  std::uint32_t venue_id{};
  AssetClass asset_class{AssetClass::Unknown};
  ProductType product_type{ProductType::Unknown};
  OptionType option_type{OptionType::Unknown};
  SettlementType settlement_type{SettlementType::Unknown};
  std::string_view canonical_symbol{};
  std::uint32_t base_asset_id{};
  std::uint32_t quote_asset_id{};
  std::uint32_t underlying_instrument_id{};
  std::int32_t price_scale{};
  std::int32_t quantity_scale{};
  std::int32_t notional_scale{};
  std::int64_t contract_multiplier{};
  std::int64_t strike_price{};
  std::uint32_t expiry_yyyymmdd{};
};

} // namespace md::refdata

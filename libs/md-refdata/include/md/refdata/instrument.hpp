#pragma once

#include <cstdint>
#include <string_view>

namespace md::refdata {

enum class AssetClass : std::uint8_t {
    Unknown = 0,
    CryptoSpot,
    CryptoPerp,
    CNEquity,
    USEquity,
    Futures,
    Options,
};

struct Instrument {
    std::uint32_t instrument_id{};
    std::uint32_t venue_id{};
    AssetClass asset_class{AssetClass::Unknown};
    std::string_view canonical_symbol{};
    std::int32_t price_scale{};
    std::int32_t quantity_scale{};
};

} // namespace md::refdata

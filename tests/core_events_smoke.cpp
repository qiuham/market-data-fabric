#include "md/core/events.hpp"
#include "md/refdata/instrument.hpp"

#include <cassert>
#include <type_traits>

int main() {
  static_assert(std::is_trivially_copyable_v<md::core::Trade>);
  static_assert(std::is_trivially_copyable_v<md::core::BookLevelUpdate>);
  static_assert(std::is_trivially_copyable_v<md::core::BookUpdate<8>>);
  static_assert(std::is_standard_layout_v<md::core::BookUpdate<8>>);

  md::core::Trade trade{};
  assert(trade.header.payload_kind == md::core::PayloadKind::Trade);
  trade.aggressor_side = md::core::AggressorSide::Buy;
  assert(trade.aggressor_side == md::core::AggressorSide::Buy);

  md::core::BookUpdate<2> update{};
  assert(update.header.payload_kind == md::core::PayloadKind::BookUpdate);
  assert(update.capacity() == 2);
  update.first_exchange_seq = 10;
  update.last_exchange_seq = 12;
  update.prev_exchange_seq = 9;
  update.flags = md::core::book_update_flag(
                     md::core::BookUpdateFlag::ChecksumPresent) |
                 md::core::book_update_flag(
                     md::core::BookUpdateFlag::PrevSeqPresent);
  update.checksum = -12345;
  assert(update.add_level(md::core::Side::Buy, md::core::BookAction::Update,
                          1000000, 2500));
  assert(update.add_level(md::core::Side::Sell, md::core::BookAction::Delete,
                          1000100, 0));
  assert(!update.add_level(md::core::Side::Sell, md::core::BookAction::Update,
                           1000200, 100));
  assert(update.level_count == 2);
  assert(update.levels[0].side == md::core::Side::Buy);
  assert(update.levels[1].action == md::core::BookAction::Delete);

  md::core::BookSnapshot<400> snapshot{};
  snapshot.bid_count = 400;
  snapshot.ask_count = 400;
  assert(snapshot.header.payload_kind == md::core::PayloadKind::BookSnapshot);

  md::refdata::Instrument btc_perp{};
  btc_perp.asset_class = md::refdata::AssetClass::Crypto;
  btc_perp.product_type = md::refdata::ProductType::Perpetual;
  btc_perp.canonical_symbol = "BTC-USDT-PERP";
  btc_perp.price_scale = 2;
  btc_perp.quantity_scale = 6;
  assert(btc_perp.asset_class == md::refdata::AssetClass::Crypto);
  assert(btc_perp.product_type == md::refdata::ProductType::Perpetual);

  return 0;
}

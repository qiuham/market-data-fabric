#pragma once

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>

namespace md::adapters::crypto::binance {

inline std::string normalize_symbol(std::string_view symbol) {
  if (symbol.empty()) {
    throw std::invalid_argument("Binance symbol 不能为空");
  }

  std::string normalized(symbol);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char ch) {
                   if (ch >= 'A' && ch <= 'Z') {
                     return static_cast<char>(ch - 'A' + 'a');
                   }
                   return static_cast<char>(ch);
                 });
  return normalized;
}

} // 命名空间 md::adapters::crypto::binance

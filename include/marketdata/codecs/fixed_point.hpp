#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace md::codecs {

inline constexpr std::int32_t kMaxFixedPointScale = 18;

[[nodiscard]] inline bool parse_decimal_to_fixed(std::string_view text,
                                                 std::int32_t scale,
                                                 std::int64_t &out) noexcept {
  if (text.empty() || scale < 0 || scale > kMaxFixedPointScale) {
    return false;
  }

  std::size_t pos = 0;
  bool negative = false;
  if (text[pos] == '+' || text[pos] == '-') {
    negative = text[pos] == '-';
    ++pos;
    if (pos == text.size()) {
      return false;
    }
  }

  const auto max_i64 =
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  const auto limit = negative ? max_i64 + 1u : max_i64;

  std::uint64_t value = 0;
  std::int32_t fractional_digits = 0;
  bool seen_digit = false;
  bool seen_decimal_point = false;

  const auto append_digit = [&](std::uint8_t digit) noexcept {
    if (value > (limit - digit) / 10u) {
      return false;
    }
    value = (value * 10u) + digit;
    return true;
  };

  for (; pos < text.size(); ++pos) {
    const char ch = text[pos];
    if (ch >= '0' && ch <= '9') {
      seen_digit = true;
      const auto digit = static_cast<std::uint8_t>(ch - '0');
      if (seen_decimal_point) {
        if (fractional_digits < scale) {
          if (!append_digit(digit)) {
            return false;
          }
          ++fractional_digits;
        } else if (digit != 0) {
          return false;
        }
      } else if (!append_digit(digit)) {
        return false;
      }
      continue;
    }

    if (ch == '.' && !seen_decimal_point) {
      seen_decimal_point = true;
      continue;
    }

    return false;
  }

  if (!seen_digit) {
    return false;
  }

  while (fractional_digits < scale) {
    if (!append_digit(0)) {
      return false;
    }
    ++fractional_digits;
  }

  if (negative) {
    if (value == limit) {
      out = std::numeric_limits<std::int64_t>::min();
    } else {
      out = -static_cast<std::int64_t>(value);
    }
  } else {
    out = static_cast<std::int64_t>(value);
  }
  return true;
}

} // 命名空间 md::codecs

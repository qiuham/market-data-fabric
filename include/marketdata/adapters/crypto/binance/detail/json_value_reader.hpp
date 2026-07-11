#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace md::adapters::binance::detail {

[[nodiscard]] inline bool ascii_equal_ignore_case(std::string_view lhs,
                                                  std::string_view rhs) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    char left = lhs[i];
    char right = rhs[i];
    if (left >= 'A' && left <= 'Z') {
      left = static_cast<char>(left - 'A' + 'a');
    }
    if (right >= 'A' && right <= 'Z') {
      right = static_cast<char>(right - 'A' + 'a');
    }
    if (left != right) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] inline bool is_json_ws(char ch) noexcept {
  return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
}

[[nodiscard]] inline bool find_json_value(std::string_view payload,
                                          std::string_view key,
                                          std::string_view &value,
                                          bool &quoted) noexcept {
  if (key.empty()) {
    return false;
  }

  std::size_t pos = 0;
  while (pos < payload.size()) {
    const auto quote = payload.find('"', pos);
    if (quote == std::string_view::npos || quote + 1 >= payload.size()) {
      return false;
    }

    const auto key_start = quote + 1;
    const auto key_end = payload.find('"', key_start);
    if (key_end == std::string_view::npos) {
      return false;
    }

    std::size_t after_key = key_end + 1;
    while (after_key < payload.size() && is_json_ws(payload[after_key])) {
      ++after_key;
    }
    if (after_key >= payload.size() || payload[after_key] != ':') {
      pos = key_end + 1;
      continue;
    }

    if (payload.substr(key_start, key_end - key_start) != key) {
      pos = key_end + 1;
      continue;
    }

    std::size_t value_start = after_key + 1;
    while (value_start < payload.size() && is_json_ws(payload[value_start])) {
      ++value_start;
    }
    if (value_start >= payload.size()) {
      return false;
    }

    quoted = payload[value_start] == '"';
    if (quoted) {
      ++value_start;
      std::size_t value_end = value_start;
      while (value_end < payload.size()) {
        if (payload[value_end] == '"' &&
            (value_end == value_start || payload[value_end - 1] != '\\')) {
          value = payload.substr(value_start, value_end - value_start);
          return true;
        }
        ++value_end;
      }
      return false;
    }

    std::size_t value_end = value_start;
    while (value_end < payload.size() && payload[value_end] != ',' &&
           payload[value_end] != '}' && !is_json_ws(payload[value_end])) {
      ++value_end;
    }
    value = payload.substr(value_start, value_end - value_start);
    return !value.empty();
  }

  return false;
}

[[nodiscard]] inline bool read_json_string(std::string_view payload,
                                           std::string_view key,
                                           std::string_view &value) noexcept {
  bool quoted = false;
  return find_json_value(payload, key, value, quoted) && quoted;
}

[[nodiscard]] inline bool read_json_bool(std::string_view payload,
                                         std::string_view key,
                                         bool &value) noexcept {
  std::string_view raw{};
  bool quoted = false;
  if (!find_json_value(payload, key, raw, quoted) || quoted) {
    return false;
  }
  if (raw == "true") {
    value = true;
    return true;
  }
  if (raw == "false") {
    value = false;
    return true;
  }
  return false;
}

[[nodiscard]] inline bool read_json_u64(std::string_view payload,
                                        std::string_view key,
                                        std::uint64_t &value) noexcept {
  std::string_view raw{};
  bool quoted = false;
  if (!find_json_value(payload, key, raw, quoted) || raw.empty()) {
    return false;
  }

  std::uint64_t parsed = 0;
  for (char ch : raw) {
    if (ch < '0' || ch > '9') {
      return false;
    }
    const auto digit = static_cast<std::uint64_t>(ch - '0');
    if (parsed > (std::numeric_limits<std::uint64_t>::max() - digit) / 10u) {
      return false;
    }
    parsed = (parsed * 10u) + digit;
  }
  value = parsed;
  return true;
}

} // 命名空间 md::adapters::binance::detail

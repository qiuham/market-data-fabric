#pragma once

#include "marketdata/runtime/logging.hpp"

#include <cstdint>
#include <string_view>

namespace md::apps::md_node {

[[nodiscard]] inline std::string_view option_value(
    std::string_view arg, std::string_view name) noexcept {
  if (!arg.starts_with(name)) {
    return {};
  }
  return arg.substr(name.size());
}

[[nodiscard]] inline bool parse_u64(std::string_view text,
                                    std::uint64_t &value) noexcept {
  if (text.empty()) {
    return false;
  }
  std::uint64_t parsed = 0;
  for (char ch : text) {
    if (ch < '0' || ch > '9') {
      return false;
    }
    parsed = (parsed * 10) + static_cast<std::uint64_t>(ch - '0');
  }
  value = parsed;
  return true;
}

[[nodiscard]] inline bool parse_log_level(
    std::string_view text, md::runtime::LogLevel &level) noexcept {
  if (text == "trace") {
    level = md::runtime::LogLevel::Trace;
    return true;
  }
  if (text == "debug") {
    level = md::runtime::LogLevel::Debug;
    return true;
  }
  if (text == "info") {
    level = md::runtime::LogLevel::Info;
    return true;
  }
  if (text == "warn") {
    level = md::runtime::LogLevel::Warn;
    return true;
  }
  if (text == "error") {
    level = md::runtime::LogLevel::Error;
    return true;
  }
  if (text == "off") {
    level = md::runtime::LogLevel::Off;
    return true;
  }
  return false;
}

[[nodiscard]] inline md::runtime::LogConfig parse_log_config(
    int argc, char **argv) noexcept {
  md::runtime::LogConfig config{};
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg{argv[i]};
    if (const auto value = option_value(arg, "--log-level="); !value.empty()) {
      md::runtime::LogLevel level{};
      if (parse_log_level(value, level)) {
        config.level = level;
      }
    }
  }
  return config;
}

} // 命名空间 md::apps::md_node

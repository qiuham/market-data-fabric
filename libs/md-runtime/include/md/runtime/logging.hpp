#pragma once

#include <cstdint>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#if MDF_RUNTIME_ENABLE_QUILL
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/core/PatternFormatterOptions.h>
#include <quill/sinks/ConsoleSink.h>
#endif

namespace md::runtime {

enum class LogLevel : std::uint8_t {
  Trace = 0,
  Debug = 1,
  Info = 2,
  Warn = 3,
  Error = 4,
  Critical = 5,
  Off = 6,
};

struct LogConfig {
  LogLevel level{LogLevel::Info};
  std::string_view pattern{"%(time) [%(log_level)] %(message)"};
  std::string_view time_pattern{"%H:%M:%S.%Qms"};
};

namespace detail {

#if MDF_RUNTIME_ENABLE_QUILL
inline quill::LogLevel to_quill_level(LogLevel level) noexcept {
  switch (level) {
  case LogLevel::Trace:
    return quill::LogLevel::TraceL3;
  case LogLevel::Debug:
    return quill::LogLevel::Debug;
  case LogLevel::Info:
    return quill::LogLevel::Info;
  case LogLevel::Warn:
    return quill::LogLevel::Warning;
  case LogLevel::Error:
    return quill::LogLevel::Error;
  case LogLevel::Critical:
    return quill::LogLevel::Critical;
  case LogLevel::Off:
    return quill::LogLevel::None;
  }
  return quill::LogLevel::Info;
}
#else
inline LogLevel &fallback_level() noexcept {
  static LogLevel level = LogLevel::Info;
  return level;
}

inline std::mutex &fallback_mutex() noexcept {
  static std::mutex mutex;
  return mutex;
}

inline std::string_view level_name(LogLevel level) noexcept {
  switch (level) {
  case LogLevel::Trace:
    return "trace";
  case LogLevel::Debug:
    return "debug";
  case LogLevel::Info:
    return "info";
  case LogLevel::Warn:
    return "warn";
  case LogLevel::Error:
    return "error";
  case LogLevel::Critical:
    return "critical";
  case LogLevel::Off:
    return "off";
  }
  return "info";
}

inline bool should_log(LogLevel message_level) noexcept {
  return fallback_level() != LogLevel::Off &&
         static_cast<std::uint8_t>(message_level) >=
             static_cast<std::uint8_t>(fallback_level());
}

template <typename T> inline std::string stringify(T &&value) {
  std::ostringstream out;
  out << std::forward<T>(value);
  return out.str();
}

inline void append_formatted(std::ostringstream &out,
                             std::string_view fmt) {
  out << fmt;
}

template <typename T, typename... Rest>
inline void append_formatted(std::ostringstream &out, std::string_view fmt,
                             T &&value, Rest &&...rest) {
  const auto placeholder = fmt.find("{}");
  if (placeholder == std::string_view::npos) {
    out << fmt << ' ' << stringify(std::forward<T>(value));
    ((out << ' ' << stringify(std::forward<Rest>(rest))), ...);
    return;
  }

  out << fmt.substr(0, placeholder) << stringify(std::forward<T>(value));
  append_formatted(out, fmt.substr(placeholder + 2),
                   std::forward<Rest>(rest)...);
}
#endif

} // namespace detail

#if MDF_RUNTIME_ENABLE_QUILL
inline quill::Logger *logger() {
  static quill::Logger *instance = [] {
    quill::Backend::start();
    auto sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>(
        "mdf_console_sink");
    auto *created = quill::Frontend::create_or_get_logger(
        "mdf", std::move(sink),
        quill::PatternFormatterOptions{"%(time) [%(log_level)] %(message)",
                                       "%H:%M:%S.%Qms"});
    created->set_log_level(quill::LogLevel::Info);
    return created;
  }();
  return instance;
}
#endif

inline void init_logging(const LogConfig &config = {}) {
#if MDF_RUNTIME_ENABLE_QUILL
  auto *root = logger();
  root->set_log_level(detail::to_quill_level(config.level));
  (void)config.pattern;
  (void)config.time_pattern;
#else
  detail::fallback_level() = config.level;
  (void)config.pattern;
  (void)config.time_pattern;
#endif
}

inline void shutdown_logging() {
#if MDF_RUNTIME_ENABLE_QUILL
  quill::Backend::stop();
#endif
}

#if !MDF_RUNTIME_ENABLE_QUILL
template <typename... Args>
inline void fallback_log(LogLevel level, std::string_view fmt,
                         Args &&...args) {
  if (!detail::should_log(level)) {
    return;
  }

  std::ostringstream message;
  detail::append_formatted(message, fmt, std::forward<Args>(args)...);

  std::lock_guard<std::mutex> lock(detail::fallback_mutex());
  std::clog << '[' << detail::level_name(level) << "] " << message.str() << '\n';
}
#endif

} // namespace md::runtime

#if MDF_RUNTIME_ENABLE_QUILL
#define MDF_LOG_TRACE(fmt, ...)                                                     \
  QUILL_LOG_TRACE_L3(::md::runtime::logger(), fmt, ##__VA_ARGS__)
#define MDF_LOG_DEBUG(fmt, ...)                                                     \
  QUILL_LOG_DEBUG(::md::runtime::logger(), fmt, ##__VA_ARGS__)
#define MDF_LOG_INFO(fmt, ...)                                                      \
  QUILL_LOG_INFO(::md::runtime::logger(), fmt, ##__VA_ARGS__)
#define MDF_LOG_WARN(fmt, ...)                                                      \
  QUILL_LOG_WARNING(::md::runtime::logger(), fmt, ##__VA_ARGS__)
#define MDF_LOG_ERROR(fmt, ...)                                                     \
  QUILL_LOG_ERROR(::md::runtime::logger(), fmt, ##__VA_ARGS__)
#define MDF_LOG_CRITICAL(fmt, ...)                                                  \
  QUILL_LOG_CRITICAL(::md::runtime::logger(), fmt, ##__VA_ARGS__)
#else
#define MDF_LOG_TRACE(fmt, ...)                                                     \
  ::md::runtime::fallback_log(::md::runtime::LogLevel::Trace, fmt, ##__VA_ARGS__)
#define MDF_LOG_DEBUG(fmt, ...)                                                     \
  ::md::runtime::fallback_log(::md::runtime::LogLevel::Debug, fmt, ##__VA_ARGS__)
#define MDF_LOG_INFO(fmt, ...)                                                      \
  ::md::runtime::fallback_log(::md::runtime::LogLevel::Info, fmt, ##__VA_ARGS__)
#define MDF_LOG_WARN(fmt, ...)                                                      \
  ::md::runtime::fallback_log(::md::runtime::LogLevel::Warn, fmt, ##__VA_ARGS__)
#define MDF_LOG_ERROR(fmt, ...)                                                     \
  ::md::runtime::fallback_log(::md::runtime::LogLevel::Error, fmt, ##__VA_ARGS__)
#define MDF_LOG_CRITICAL(fmt, ...)                                                  \
  ::md::runtime::fallback_log(::md::runtime::LogLevel::Critical, fmt, ##__VA_ARGS__)
#endif

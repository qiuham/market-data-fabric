#pragma once

#include <string_view>

namespace md::apps::md_node {

[[nodiscard]] bool is_binance_command(std::string_view command) noexcept;
[[nodiscard]] int run_binance_command(std::string_view command, int argc,
                                      char **argv);
void log_binance_usage();

} // 命名空间 md::apps::md_node

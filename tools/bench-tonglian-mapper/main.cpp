#include "marketdata/adapters/tonglian/mapper.hpp"

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace tl = md::adapters::tonglian;

int main(int argc, char** argv) {
  std::uint64_t iterations = 20'000'000;
  if (argc == 2) {
    const std::string_view text = argv[1];
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), iterations);
    if (parsed.ec != std::errc{} || iterations == 0) {
      std::cerr << "用法: " << argv[0] << " [迭代次数]\n";
      return 2;
    }
  }

  tl::TonglianSequenceGate gate;
  if (!gate.complete_recovery(3, 0)) {
    std::cerr << "无法建立序号基线\n";
    return 1;
  }
  const auto start = std::chrono::steady_clock::now();
  for (std::uint64_t sequence = 1; sequence <= iterations; ++sequence) {
    if (!gate.observe(3, sequence).accepted()) {
      std::cerr << "序号被意外拒绝: " << sequence << '\n';
      return 1;
    }
  }
  const auto elapsed = std::chrono::steady_clock::now() - start;
  const auto nanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
  const auto ns_per_event =
      static_cast<double>(nanoseconds) / static_cast<double>(iterations);
  const auto events_per_second = 1'000'000'000.0 / ns_per_event;

  std::cout << "通联序号检查基准\n"
            << "事件数\t" << iterations << '\n'
            << "每事件纳秒\t" << std::fixed << std::setprecision(2)
            << ns_per_event << '\n'
            << "每秒事件数\t" << std::setprecision(0)
            << events_per_second << '\n'
            << "接受事件数\t" << gate.stats().accepted << '\n';
  return gate.last_accepted() == iterations ? 0 : 1;
}

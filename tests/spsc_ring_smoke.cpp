#include "md/core/message.hpp"
#include "md/runtime/spsc_ring.hpp"

#include <cassert>
#include <cstdint>
#include <string_view>
#include <thread>

int main() {
  md::runtime::SpscRing<std::uint64_t, 8> numbers{};
  assert(numbers.empty());
  for (std::uint64_t value = 0; value < numbers.capacity(); ++value) {
    assert(numbers.try_push(value));
  }
  assert(numbers.full());
  assert(!numbers.try_push(9));

  for (std::uint64_t expected = 0; expected < numbers.capacity(); ++expected) {
    std::uint64_t actual = 999;
    assert(numbers.try_pop(actual));
    assert(actual == expected);
  }
  assert(numbers.empty());

  md::runtime::SpscRing<md::core::RawProviderMessageFrame<256>, 16> frames{};
  md::core::RawProviderMessageFrame<256> frame{};
  frame.envelope.payload_kind = md::core::PayloadKind::RawProviderMessage;
  frame.envelope.payload_encoding = md::core::PayloadEncoding::ProviderJson;
  assert(md::core::assign_payload(frame, std::string_view{"hello"}));
  assert(frames.try_push(frame));

  md::core::RawProviderMessageFrame<256> popped{};
  assert(frames.try_pop(popped));
  assert(popped.envelope.payload_size == 5);
  assert(popped.payload.text_view() == "hello");

  md::runtime::SpscRing<std::uint64_t, 1024> threaded{};
  constexpr std::uint64_t kCount = 100000;
  std::thread producer([&] {
    for (std::uint64_t value = 0; value < kCount; ++value) {
      while (!threaded.try_push(value)) {
      }
    }
  });
  std::thread consumer([&] {
    for (std::uint64_t expected = 0; expected < kCount; ++expected) {
      std::uint64_t actual = 0;
      while (!threaded.try_pop(actual)) {
      }
      assert(actual == expected);
    }
  });
  producer.join();
  consumer.join();
  assert(threaded.empty());

  return 0;
}

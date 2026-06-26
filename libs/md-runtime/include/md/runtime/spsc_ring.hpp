#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace md::runtime {

inline constexpr std::size_t kCacheLineSize = 64;

template <std::size_t Value>
inline constexpr bool is_power_of_two_v = Value != 0 &&
                                          (Value & (Value - 1)) == 0;

// 单生产者单消费者 ring buffer。只能有一个线程调用 push API，
// 也只能有另一个线程调用 pop API。
template <typename T, std::size_t Capacity> class SpscRing {
  static_assert(Capacity >= 2, "SpscRing capacity must be at least 2");
  static_assert(is_power_of_two_v<Capacity>,
                "SpscRing capacity must be a power of two");
  static_assert(std::is_default_constructible_v<T>,
                "SpscRing value type must be default constructible");
  static_assert(std::is_move_assignable_v<T>,
                "SpscRing value type must be move assignable");

public:
  using value_type = T;
  static constexpr std::size_t kCapacity = Capacity;

  SpscRing() = default;
  SpscRing(const SpscRing &) = delete;
  SpscRing &operator=(const SpscRing &) = delete;

  [[nodiscard]] static constexpr std::size_t capacity() noexcept {
    return Capacity;
  }

  bool try_push(const T &value) noexcept(std::is_nothrow_copy_assignable_v<T>) {
    return push_impl(value);
  }

  bool try_push(T &&value) noexcept(std::is_nothrow_move_assignable_v<T>) {
    return push_impl(std::move(value));
  }

  template <typename... Args>
  bool try_emplace(Args &&...args) noexcept(
      std::is_nothrow_constructible_v<T, Args...>
          &&std::is_nothrow_move_assignable_v<T>) {
    return try_push(T{std::forward<Args>(args)...});
  }

  bool try_pop(T &out) noexcept(std::is_nothrow_move_assignable_v<T>) {
    const auto read = read_index_.value.load(std::memory_order_relaxed);
    const auto write = write_index_.value.load(std::memory_order_acquire);
    if (read == write) {
      return false;
    }

    auto &slot = buffer_[read & kIndexMask];
    out = std::move(slot);
    slot = T{};
    read_index_.value.store(read + 1, std::memory_order_release);
    return true;
  }

  [[nodiscard]] bool empty() const noexcept {
    const auto read = read_index_.value.load(std::memory_order_acquire);
    const auto write = write_index_.value.load(std::memory_order_acquire);
    return read == write;
  }

  [[nodiscard]] bool full() const noexcept {
    const auto write = write_index_.value.load(std::memory_order_acquire);
    const auto read = read_index_.value.load(std::memory_order_acquire);
    return write - read >= Capacity;
  }

  [[nodiscard]] std::size_t size_estimate() const noexcept {
    const auto write = write_index_.value.load(std::memory_order_acquire);
    const auto read = read_index_.value.load(std::memory_order_acquire);
    return static_cast<std::size_t>(write - read);
  }

private:
  template <typename U>
  bool push_impl(U &&value) noexcept(
      noexcept(std::declval<T &>() = std::forward<U>(value))) {
    const auto write = write_index_.value.load(std::memory_order_relaxed);
    const auto read = read_index_.value.load(std::memory_order_acquire);
    if (write - read >= Capacity) {
      return false;
    }

    buffer_[write & kIndexMask] = std::forward<U>(value);
    write_index_.value.store(write + 1, std::memory_order_release);
    return true;
  }

  struct alignas(kCacheLineSize) PaddedAtomicIndex {
    std::atomic<std::uint64_t> value{0};
  };

  static constexpr std::uint64_t kIndexMask = Capacity - 1;

  alignas(kCacheLineSize) std::array<T, Capacity> buffer_{};
  PaddedAtomicIndex write_index_{};
  PaddedAtomicIndex read_index_{};
};

} // 命名空间 md::runtime

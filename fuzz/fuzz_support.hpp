#ifndef FEEDFORGE_FUZZ_SUPPORT_HPP
#define FEEDFORGE_FUZZ_SUPPORT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

namespace feedforge::fuzz {

[[noreturn]] inline void invariant_failure() noexcept {
#if defined(__clang__) || defined(__GNUC__)
  __builtin_trap();
#else
  std::abort();
#endif
}

inline void require(const bool condition) noexcept {
  if (!condition) {
    invariant_failure();
  }
}

inline constexpr unsigned int invalid_hex_digit = 16U;

[[nodiscard]] inline constexpr unsigned int hex_value(const std::uint8_t value) noexcept {
  if (value >= static_cast<std::uint8_t>('0') && value <= static_cast<std::uint8_t>('9')) {
    return static_cast<unsigned int>(value - static_cast<std::uint8_t>('0'));
  }
  if (value >= static_cast<std::uint8_t>('a') && value <= static_cast<std::uint8_t>('f')) {
    return 10U + static_cast<unsigned int>(value - static_cast<std::uint8_t>('a'));
  }
  if (value >= static_cast<std::uint8_t>('A') && value <= static_cast<std::uint8_t>('F')) {
    return 10U + static_cast<unsigned int>(value - static_cast<std::uint8_t>('A'));
  }
  return invalid_hex_digit;
}

[[nodiscard]] inline constexpr bool is_ascii_space(const std::uint8_t value) noexcept {
  return value == static_cast<std::uint8_t>(' ') || value == static_cast<std::uint8_t>('\t') ||
         value == static_cast<std::uint8_t>('\r') || value == static_cast<std::uint8_t>('\n');
}

template <std::size_t Capacity>
[[nodiscard]] bool decode_hex_seed(const std::span<const std::uint8_t> encoded,
                                   std::array<std::byte, Capacity>& storage,
                                   std::span<const std::byte>& decoded) noexcept {
  constexpr std::array<std::uint8_t, 4U> prefix{
      static_cast<std::uint8_t>('h'),
      static_cast<std::uint8_t>('e'),
      static_cast<std::uint8_t>('x'),
      static_cast<std::uint8_t>(':'),
  };

  if (encoded.size() < prefix.size()) {
    return false;
  }
  for (std::size_t index = 0U; index < prefix.size(); ++index) {
    if (encoded[index] != prefix[index]) {
      return false;
    }
  }

  unsigned int high_nibble = invalid_hex_digit;
  std::size_t output_size = 0U;
  for (std::size_t index = prefix.size(); index < encoded.size(); ++index) {
    const std::uint8_t current = encoded[index];
    if (is_ascii_space(current)) {
      continue;
    }

    const unsigned int value = hex_value(current);
    if (value == invalid_hex_digit) {
      return false;
    }
    if (high_nibble == invalid_hex_digit) {
      high_nibble = value;
      continue;
    }
    if (output_size == storage.size()) {
      return false;
    }

    storage[output_size] = std::byte{static_cast<unsigned char>((high_nibble << 4U) | value)};
    ++output_size;
    high_nibble = invalid_hex_digit;
  }

  if (high_nibble != invalid_hex_digit) {
    return false;
  }

  decoded = std::span<const std::byte>{storage.data(), output_size};
  return true;
}

template <class Function> int run_standalone_smoke(Function function) noexcept {
  std::array<std::uint8_t, 512U> bytes{};
  static_cast<void>(function(bytes.data(), 0U));

  std::uint32_t state = 0x6d2b79f5U;
  for (std::size_t size = 1U; size <= bytes.size(); ++size) {
    state = state * 1664525U + 1013904223U;
    bytes[size - 1U] = static_cast<std::uint8_t>(state >> 24U);
    static_cast<void>(function(bytes.data(), size));
  }

  bytes.fill(0U);
  static_cast<void>(function(bytes.data(), bytes.size()));
  bytes.fill(0xffU);
  static_cast<void>(function(bytes.data(), bytes.size()));
  return 0;
}

} // namespace feedforge::fuzz

#endif // FEEDFORGE_FUZZ_SUPPORT_HPP

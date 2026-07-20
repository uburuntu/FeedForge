#include "itch50_differential.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>

namespace {

namespace reference = feedforge_reference::itch50;

int failures = 0;

void check(const bool condition, const std::string_view description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    ++failures;
  }
}

void check_outcome(const reference::decode_outcome& actual, const reference::decode_status status,
                   const std::byte message_type, const std::uint16_t expected_size,
                   const std::size_t actual_size, const std::string_view description) {
  check(actual.status == status && actual.message_type == message_type &&
            actual.expected_size == expected_size && actual.actual_size == actual_size,
        description);
}

void test_reference_primitives() {
  constexpr std::array bytes{
      std::byte{0x01U}, std::byte{0x23U}, std::byte{0x45U}, std::byte{0x67U},
      std::byte{0x89U}, std::byte{0xabU}, std::byte{0xcdU}, std::byte{0xefU},
  };
  constexpr auto whole = reference::read_unsigned(bytes, 0U, 8U);
  static_assert(whole.valid && whole.value == UINT64_C(0x0123456789abcdef));
  constexpr auto middle = reference::read_unsigned(bytes, 2U, 4U);
  static_assert(middle.valid && middle.value == UINT64_C(0x456789ab));
  static_assert(!reference::read_unsigned(bytes, 0U, 0U).valid);
  static_assert(!reference::read_unsigned(bytes, 1U, 8U).valid);
  static_assert(!reference::read_unsigned(bytes, 9U, 1U).valid);

  constexpr std::array byte_text{std::byte{0U}, std::byte{0x80U}, std::byte{0xffU}, std::byte{' '}};
  constexpr std::array char_text{static_cast<char>(0x80U), static_cast<char>(0xffU)};
  static_assert(reference::bytes_equal(byte_text, 1U, char_text));
  static_assert(!reference::bytes_equal(byte_text, 2U, char_text));
  static_assert(!reference::bytes_equal(byte_text, 4U, char_text));

  constexpr std::span<const std::byte> empty;
  constexpr reference::decode_outcome empty_outcome = reference::classify(empty);
  static_assert(empty_outcome.status == reference::decode_status::empty_payload);

  constexpr std::array unknown{std::byte{'?'}};
  constexpr reference::decode_outcome unknown_outcome = reference::classify(unknown);
  static_assert(unknown_outcome.status == reference::decode_status::unknown_message_type);
}

void test_statuses() {
  constexpr std::span<const std::byte> empty;
  check(reference::matches_generated(empty), "empty payload differential");

  constexpr std::array unknown{std::byte{'?'}, std::byte{0x12U}, std::byte{0x34U}};
  check_outcome(reference::classify(unknown), reference::decode_status::unknown_message_type,
                std::byte{'?'}, 0U, unknown.size(), "unknown classification");
  check(reference::matches_generated(unknown), "unknown payload differential");

  std::array<std::byte, reference::maximum_message_size + 1U> storage{};
  for (const reference::message_layout& layout : reference::message_layouts) {
    storage.fill(std::byte{0x5aU});
    storage.front() = layout.discriminator;

    const auto valid = std::span<const std::byte>{storage.data(), layout.size};
    check_outcome(reference::classify(valid), reference::decode_status::emitted,
                  layout.discriminator, layout.size, layout.size, layout.name);
    check(reference::matches_generated(valid), layout.name);

    const auto short_payload = std::span<const std::byte>{storage.data(), layout.size - 1U};
    check_outcome(reference::classify(short_payload),
                  reference::decode_status::invalid_message_size, layout.discriminator, layout.size,
                  layout.size - 1U, layout.name);
    check(reference::matches_generated(short_payload), layout.name);

    const auto long_payload = std::span<const std::byte>{storage.data(), layout.size + 1U};
    check_outcome(reference::classify(long_payload), reference::decode_status::invalid_message_size,
                  layout.discriminator, layout.size, layout.size + 1U, layout.name);
    check(reference::matches_generated(long_payload), layout.name);
  }
}

[[nodiscard]] constexpr std::byte pattern_byte(const std::size_t pattern, const std::size_t index,
                                               std::uint32_t& state) noexcept {
  switch (pattern) {
  case 0U:
    return std::byte{0U};
  case 1U:
    return std::byte{0xffU};
  case 2U:
    return std::byte{static_cast<unsigned char>(index)};
  case 3U:
    return index % 2U == 0U ? std::byte{0xaaU} : std::byte{0x55U};
  default:
    state = state * 1664525U + 1013904223U;
    return std::byte{static_cast<unsigned char>(state >> 24U)};
  }
}

void test_arbitrary_values() {
  std::array<std::byte, reference::maximum_message_size> storage{};
  for (std::size_t layout_index = 0U; layout_index < reference::message_layouts.size();
       ++layout_index) {
    const reference::message_layout& layout = reference::message_layouts[layout_index];

    for (std::size_t pattern = 0U; pattern < 20U; ++pattern) {
      std::uint32_t state = 0x9e3779b9U ^ static_cast<std::uint32_t>(layout_index * 257U + pattern);
      for (std::size_t index = 0U; index < layout.size; ++index) {
        storage[index] = pattern_byte(pattern, index, state);
      }
      storage.front() = layout.discriminator;
      const auto payload = std::span<const std::byte>{storage.data(), layout.size};
      check(reference::matches_generated(payload), layout.name);
    }

    for (std::size_t changed = 1U; changed < layout.size; ++changed) {
      for (std::size_t index = 0U; index < layout.size; ++index) {
        storage[index] = std::byte{static_cast<unsigned char>(index * 37U + layout_index * 13U)};
      }
      storage.front() = layout.discriminator;
      storage[changed] ^= std::byte{0xffU};
      const auto payload = std::span<const std::byte>{storage.data(), layout.size};
      check(reference::matches_generated(payload), layout.name);
    }
  }
}

} // namespace

int main() {
  test_reference_primitives();
  test_statuses();
  test_arbitrary_values();
  return failures == 0 ? 0 : 1;
}

#include <feedforge/framing/binary_file.hpp>

#include "fuzz_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>

namespace {

[[noreturn]] void invariant_failure() noexcept { feedforge::fuzz::invariant_failure(); }

void require(bool condition) noexcept {
  if (!condition) {
    invariant_failure();
  }
}

constexpr std::uint64_t as_offset(std::size_t value) noexcept {
  return static_cast<std::uint64_t>(value);
}

bool same_span(std::span<const std::byte> lhs, std::span<const std::byte> rhs) noexcept {
  return lhs.size() == rhs.size() && (lhs.empty() || lhs.data() == rhs.data());
}

bool same_outcome(const feedforge::frame_outcome& lhs,
                  const feedforge::frame_outcome& rhs) noexcept {
  return lhs.status == rhs.status && lhs.error == rhs.error && lhs.offset == rhs.offset &&
         same_span(lhs.frame.payload, rhs.frame.payload) &&
         lhs.frame.file_offset == rhs.frame.file_offset && lhs.frame.ordinal == rhs.frame.ordinal;
}

void require_empty_frame(const feedforge::frame_outcome& outcome) noexcept {
  require(outcome.frame.payload.empty());
  require(outcome.frame.file_offset == 0U);
  require(outcome.frame.ordinal == 0U);
}

void require_sticky_terminal(feedforge::binary_file_cursor& cursor,
                             const feedforge::frame_outcome& terminal,
                             std::size_t expected_consumed,
                             std::span<const std::byte> expected_remaining = {}) noexcept {
  require(terminal.status != feedforge::frame_status::frame);
  require_empty_frame(terminal);

  for (unsigned int repetition = 0; repetition < 2U; ++repetition) {
    require(cursor.consumed() == expected_consumed);
    const auto remaining = cursor.remaining();
    require(remaining.size() == expected_remaining.size());
    if (terminal.status == feedforge::frame_status::complete || !expected_remaining.empty()) {
      require(remaining.data() == expected_remaining.data());
    }
    require(same_outcome(cursor.next(), terminal));
  }
}

void exercise_cursor(std::span<const std::byte> input) noexcept {
  feedforge::binary_file_cursor cursor{input};
  feedforge::binary_file_cursor mirror{input};
  std::size_t frame_count = 0U;

  require(cursor.consumed() == 0U);
  require(cursor.remaining().empty());

  for (;;) {
    require(frame_count <= input.size() / 3U);

    const auto prefix_offset = cursor.consumed();
    require(prefix_offset == mirror.consumed());
    require(prefix_offset <= input.size());
    require(cursor.remaining().empty());
    require(mirror.remaining().empty());

    const auto outcome = cursor.next();
    const auto mirror_outcome = mirror.next();
    require(same_outcome(outcome, mirror_outcome));
    require(cursor.consumed() == mirror.consumed());
    require(same_span(cursor.remaining(), mirror.remaining()));
    require(outcome.offset == as_offset(prefix_offset));

    const auto available = input.size() - prefix_offset;
    switch (outcome.status) {
    case feedforge::frame_status::frame: {
      require(outcome.error == feedforge::framing_errc::none);
      require(available >= 3U);

      const auto high = std::to_integer<unsigned int>(input[prefix_offset]);
      const auto low = std::to_integer<unsigned int>(input[prefix_offset + 1U]);
      const auto declared_size = static_cast<std::size_t>((high << 8U) | low);

      require(declared_size != 0U);
      require(declared_size <= available - 2U);
      require(outcome.frame.file_offset == as_offset(prefix_offset));
      require(outcome.frame.ordinal == as_offset(frame_count));
      require(outcome.frame.payload.size() == declared_size);
      require(outcome.frame.payload.data() == input.data() + prefix_offset + 2U);
      require(cursor.consumed() == prefix_offset + 2U + declared_size);
      require(cursor.remaining().empty());

      ++frame_count;
      break;
    }

    case feedforge::frame_status::complete: {
      require(outcome.error == feedforge::framing_errc::none);
      require(available >= 2U);
      require(input[prefix_offset] == std::byte{0});
      require(input[prefix_offset + 1U] == std::byte{0});

      const auto expected_consumed = prefix_offset + 2U;
      require_sticky_terminal(cursor, outcome, expected_consumed, input.subspan(expected_consumed));
      return;
    }

    case feedforge::frame_status::incomplete:
      require(outcome.error == feedforge::framing_errc::none);
      require(available == 0U);
      require(prefix_offset == input.size());
      require_sticky_terminal(cursor, outcome, prefix_offset);
      return;

    case feedforge::frame_status::error:
      require(outcome.error != feedforge::framing_errc::none);
      require(outcome.error != feedforge::framing_errc::trailing_data_after_end_marker);
      require(cursor.consumed() == prefix_offset);
      require(cursor.remaining().empty());

      if (outcome.error == feedforge::framing_errc::truncated_length_prefix) {
        require(available == 1U);
      } else {
        require(outcome.error == feedforge::framing_errc::truncated_payload);
        require(available >= 2U);

        const auto high = std::to_integer<unsigned int>(input[prefix_offset]);
        const auto low = std::to_integer<unsigned int>(input[prefix_offset + 1U]);
        const auto declared_size = static_cast<std::size_t>((high << 8U) | low);
        require(declared_size != 0U);
        require(declared_size > available - 2U);
      }

      require_sticky_terminal(cursor, outcome, prefix_offset);
      return;
    }
  }
}

constexpr unsigned int invalid_hex_digit = 16U;

constexpr unsigned int hex_value(std::uint8_t value) noexcept {
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

constexpr bool is_ascii_space(std::uint8_t value) noexcept {
  return value == static_cast<std::uint8_t>(' ') || value == static_cast<std::uint8_t>('\t') ||
         value == static_cast<std::uint8_t>('\r') || value == static_cast<std::uint8_t>('\n');
}

bool decode_hex_seed(std::span<const std::uint8_t> encoded, std::array<std::byte, 4096U>& storage,
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

  auto high_nibble = invalid_hex_digit;
  std::size_t output_size = 0U;
  for (std::size_t index = prefix.size(); index < encoded.size(); ++index) {
    const auto current = encoded[index];
    if (is_ascii_space(current)) {
      continue;
    }

    const auto value = hex_value(current);
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

static_assert(
    std::is_nothrow_constructible_v<feedforge::binary_file_cursor, std::span<const std::byte>>);
static_assert(noexcept(std::declval<feedforge::binary_file_cursor&>().next()));
static_assert(noexcept(std::declval<const feedforge::binary_file_cursor&>().consumed()));
static_assert(noexcept(std::declval<const feedforge::binary_file_cursor&>().remaining()));

} // namespace

int feedforge_fuzz_binary_file_input(const std::uint8_t* data, const std::size_t size) noexcept {
  const auto raw = std::span<const std::byte>{reinterpret_cast<const std::byte*>(data), size};
  exercise_cursor(raw);

  std::array<std::byte, 4096U> decoded_storage{};
  std::span<const std::byte> decoded;
  if (decode_hex_seed(std::span<const std::uint8_t>{data, size}, decoded_storage, decoded)) {
    exercise_cursor(decoded);
  }

  return 0;
}

#if defined(FEEDFORGE_FUZZ_STANDALONE)
int main() { return feedforge::fuzz::run_standalone_smoke(feedforge_fuzz_binary_file_input); }
#else
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size) noexcept {
  return feedforge_fuzz_binary_file_input(data, size);
}
#endif

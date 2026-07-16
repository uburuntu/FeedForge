#include "test_support.hpp"

#include <feedforge/framing/binary_file.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>

namespace {

constexpr std::byte as_byte(unsigned int value) noexcept {
  return std::byte{static_cast<unsigned char>(value)};
}

void assert_empty_frame(const feedforge::frame_outcome& outcome) {
  FEEDFORGE_CHECK(outcome.frame.payload.empty());
  FEEDFORGE_CHECK(outcome.frame.file_offset == 0U);
  FEEDFORGE_CHECK(outcome.frame.ordinal == 0U);
}

void assert_same_outcome(const feedforge::frame_outcome& lhs, const feedforge::frame_outcome& rhs) {
  FEEDFORGE_CHECK(lhs.status == rhs.status);
  FEEDFORGE_CHECK(lhs.error == rhs.error);
  FEEDFORGE_CHECK(lhs.offset == rhs.offset);
  FEEDFORGE_CHECK(lhs.frame.payload.data() == rhs.frame.payload.data());
  FEEDFORGE_CHECK(lhs.frame.payload.size() == rhs.frame.payload.size());
  FEEDFORGE_CHECK(lhs.frame.file_offset == rhs.frame.file_offset);
  FEEDFORGE_CHECK(lhs.frame.ordinal == rhs.frame.ordinal);
}

void assert_sticky_terminal(feedforge::binary_file_cursor& cursor,
                            const feedforge::frame_outcome& terminal, std::size_t expected_consumed,
                            std::span<const std::byte> expected_remaining = {}) {
  FEEDFORGE_CHECK(terminal.status != feedforge::frame_status::frame);
  assert_empty_frame(terminal);

  for (unsigned int repetition = 0; repetition < 2U; ++repetition) {
    FEEDFORGE_CHECK(cursor.consumed() == expected_consumed);
    const auto remaining = cursor.remaining();
    FEEDFORGE_CHECK(remaining.size() == expected_remaining.size());
    if (terminal.status == feedforge::frame_status::complete || !expected_remaining.empty()) {
      FEEDFORGE_CHECK(remaining.data() == expected_remaining.data());
    }

    const auto repeated = cursor.next();
    assert_same_outcome(terminal, repeated);
  }

  FEEDFORGE_CHECK(cursor.consumed() == expected_consumed);
  FEEDFORGE_CHECK(cursor.remaining().size() == expected_remaining.size());
}

constexpr bool compile_time_cursor_check() {
  constexpr std::array input{
      as_byte(0x00), as_byte(0x01), as_byte(0x41), as_byte(0x00), as_byte(0x00),
  };
  feedforge::binary_file_cursor cursor{std::span<const std::byte>{input}};

  const auto frame = cursor.next();
  if (frame.status != feedforge::frame_status::frame ||
      frame.error != feedforge::framing_errc::none || frame.offset != 0U ||
      frame.frame.file_offset != 0U || frame.frame.ordinal != 0U ||
      frame.frame.payload.size() != 1U || frame.frame.payload[0] != as_byte(0x41) ||
      cursor.consumed() != 3U || !cursor.remaining().empty()) {
    return false;
  }

  const auto complete = cursor.next();
  return complete.status == feedforge::frame_status::complete &&
         complete.error == feedforge::framing_errc::none && complete.offset == 3U &&
         cursor.consumed() == input.size() && cursor.remaining().empty();
}

static_assert(compile_time_cursor_check());
static_assert(std::is_same_v<std::underlying_type_t<feedforge::frame_status>, std::uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<feedforge::framing_errc>, std::uint8_t>);
static_assert(
    std::is_nothrow_constructible_v<feedforge::binary_file_cursor, std::span<const std::byte>>);
static_assert(noexcept(std::declval<feedforge::binary_file_cursor&>().next()));
static_assert(noexcept(std::declval<const feedforge::binary_file_cursor&>().consumed()));
static_assert(noexcept(std::declval<const feedforge::binary_file_cursor&>().remaining()));

void test_empty_incomplete_file() {
  constexpr std::array<std::byte, 0> input{};
  feedforge::binary_file_cursor cursor{std::span<const std::byte>{input}};

  FEEDFORGE_CHECK(cursor.consumed() == 0U);
  FEEDFORGE_CHECK(cursor.remaining().empty());

  const auto first = cursor.next();
  FEEDFORGE_CHECK(first.status == feedforge::frame_status::incomplete);
  FEEDFORGE_CHECK(first.error == feedforge::framing_errc::none);
  FEEDFORGE_CHECK(first.offset == 0U);
  assert_sticky_terminal(cursor, first, 0U);
}

void test_empty_complete_file() {
  constexpr std::array input{as_byte(0x00), as_byte(0x00)};
  feedforge::binary_file_cursor cursor{std::span<const std::byte>{input}};

  const auto first = cursor.next();
  FEEDFORGE_CHECK(first.status == feedforge::frame_status::complete);
  FEEDFORGE_CHECK(first.error == feedforge::framing_errc::none);
  FEEDFORGE_CHECK(first.offset == 0U);
  assert_sticky_terminal(cursor, first, input.size(),
                         std::span<const std::byte>{input}.subspan(input.size()));
}

void test_one_frame_incomplete_session() {
  constexpr std::array input{
      as_byte(0x00), as_byte(0x03), as_byte(0x41), as_byte(0x42), as_byte(0x43),
  };
  feedforge::binary_file_cursor cursor{std::span<const std::byte>{input}};

  const auto frame = cursor.next();
  FEEDFORGE_CHECK(frame.status == feedforge::frame_status::frame);
  FEEDFORGE_CHECK(frame.error == feedforge::framing_errc::none);
  FEEDFORGE_CHECK(frame.offset == 0U);
  FEEDFORGE_CHECK(frame.frame.file_offset == 0U);
  FEEDFORGE_CHECK(frame.frame.ordinal == 0U);
  FEEDFORGE_CHECK(frame.frame.payload.data() == input.data() + 2U);
  FEEDFORGE_CHECK(frame.frame.payload.size() == 3U);
  FEEDFORGE_CHECK(frame.frame.payload[0] == as_byte(0x41));
  FEEDFORGE_CHECK(frame.frame.payload[2] == as_byte(0x43));
  FEEDFORGE_CHECK(cursor.consumed() == input.size());
  FEEDFORGE_CHECK(cursor.remaining().empty());

  const auto incomplete = cursor.next();
  FEEDFORGE_CHECK(incomplete.status == feedforge::frame_status::incomplete);
  FEEDFORGE_CHECK(incomplete.error == feedforge::framing_errc::none);
  FEEDFORGE_CHECK(incomplete.offset == input.size());
  assert_sticky_terminal(cursor, incomplete, input.size());
}

void test_multiple_frames_and_complete_session() {
  constexpr std::array input{
      as_byte(0x00), as_byte(0x01), as_byte(0x41), as_byte(0x00), as_byte(0x02),
      as_byte(0x42), as_byte(0x43), as_byte(0x00), as_byte(0x00),
  };
  feedforge::binary_file_cursor cursor{std::span<const std::byte>{input}};

  const auto first = cursor.next();
  FEEDFORGE_CHECK(first.status == feedforge::frame_status::frame);
  FEEDFORGE_CHECK(first.error == feedforge::framing_errc::none);
  FEEDFORGE_CHECK(first.offset == 0U);
  FEEDFORGE_CHECK(first.frame.file_offset == 0U);
  FEEDFORGE_CHECK(first.frame.ordinal == 0U);
  FEEDFORGE_CHECK(first.frame.payload.size() == 1U);
  FEEDFORGE_CHECK(first.frame.payload[0] == as_byte(0x41));
  FEEDFORGE_CHECK(cursor.consumed() == 3U);
  FEEDFORGE_CHECK(cursor.remaining().empty());

  const auto second = cursor.next();
  FEEDFORGE_CHECK(second.status == feedforge::frame_status::frame);
  FEEDFORGE_CHECK(second.error == feedforge::framing_errc::none);
  FEEDFORGE_CHECK(second.offset == 3U);
  FEEDFORGE_CHECK(second.frame.file_offset == 3U);
  FEEDFORGE_CHECK(second.frame.ordinal == 1U);
  FEEDFORGE_CHECK(second.frame.payload.size() == 2U);
  FEEDFORGE_CHECK(second.frame.payload[0] == as_byte(0x42));
  FEEDFORGE_CHECK(second.frame.payload[1] == as_byte(0x43));
  FEEDFORGE_CHECK(cursor.consumed() == 7U);
  FEEDFORGE_CHECK(cursor.remaining().empty());

  const auto complete = cursor.next();
  FEEDFORGE_CHECK(complete.status == feedforge::frame_status::complete);
  FEEDFORGE_CHECK(complete.error == feedforge::framing_errc::none);
  FEEDFORGE_CHECK(complete.offset == 7U);
  assert_sticky_terminal(cursor, complete, input.size(),
                         std::span<const std::byte>{input}.subspan(input.size()));
}

void test_remaining_after_end_marker() {
  constexpr std::array input{
      as_byte(0x00), as_byte(0x01), as_byte(0x41), as_byte(0x00),
      as_byte(0x00), as_byte(0xde), as_byte(0xad),
  };
  feedforge::binary_file_cursor cursor{std::span<const std::byte>{input}};

  const auto frame = cursor.next();
  FEEDFORGE_CHECK(frame.status == feedforge::frame_status::frame);
  FEEDFORGE_CHECK(frame.error == feedforge::framing_errc::none);
  FEEDFORGE_CHECK(frame.offset == 0U);
  FEEDFORGE_CHECK(frame.frame.file_offset == 0U);
  FEEDFORGE_CHECK(frame.frame.ordinal == 0U);
  FEEDFORGE_CHECK(frame.frame.payload.size() == 1U);
  FEEDFORGE_CHECK(cursor.consumed() == 3U);
  FEEDFORGE_CHECK(cursor.remaining().empty());

  const auto complete = cursor.next();
  FEEDFORGE_CHECK(complete.status == feedforge::frame_status::complete);
  FEEDFORGE_CHECK(complete.error == feedforge::framing_errc::none);
  FEEDFORGE_CHECK(complete.offset == 3U);
  const auto trailing = std::span<const std::byte>{input}.subspan(5U);
  assert_sticky_terminal(cursor, complete, 5U, trailing);
  FEEDFORGE_CHECK(cursor.remaining()[0] == as_byte(0xde));
  FEEDFORGE_CHECK(cursor.remaining()[1] == as_byte(0xad));
}

void test_truncated_length_prefix() {
  constexpr std::array input{as_byte(0x01)};
  feedforge::binary_file_cursor cursor{std::span<const std::byte>{input}};

  const auto first = cursor.next();
  FEEDFORGE_CHECK(first.status == feedforge::frame_status::error);
  FEEDFORGE_CHECK(first.error == feedforge::framing_errc::truncated_length_prefix);
  FEEDFORGE_CHECK(first.offset == 0U);
  assert_sticky_terminal(cursor, first, 0U);
}

void test_truncated_length_prefix_after_frame() {
  constexpr std::array input{
      as_byte(0x00),
      as_byte(0x01),
      as_byte(0x41),
      as_byte(0x12),
  };
  feedforge::binary_file_cursor cursor{std::span<const std::byte>{input}};

  const auto frame = cursor.next();
  FEEDFORGE_CHECK(frame.status == feedforge::frame_status::frame);
  FEEDFORGE_CHECK(frame.error == feedforge::framing_errc::none);
  FEEDFORGE_CHECK(frame.offset == 0U);
  FEEDFORGE_CHECK(frame.frame.file_offset == 0U);
  FEEDFORGE_CHECK(frame.frame.ordinal == 0U);
  FEEDFORGE_CHECK(cursor.consumed() == 3U);
  FEEDFORGE_CHECK(cursor.remaining().empty());

  const auto error = cursor.next();
  FEEDFORGE_CHECK(error.status == feedforge::frame_status::error);
  FEEDFORGE_CHECK(error.error == feedforge::framing_errc::truncated_length_prefix);
  FEEDFORGE_CHECK(error.offset == 3U);
  assert_sticky_terminal(cursor, error, 3U);
}

void test_truncated_payload() {
  constexpr std::array input{
      as_byte(0x00),
      as_byte(0x03),
      as_byte(0x41),
      as_byte(0x42),
  };
  feedforge::binary_file_cursor cursor{std::span<const std::byte>{input}};

  const auto first = cursor.next();
  FEEDFORGE_CHECK(first.status == feedforge::frame_status::error);
  FEEDFORGE_CHECK(first.error == feedforge::framing_errc::truncated_payload);
  FEEDFORGE_CHECK(first.offset == 0U);
  assert_sticky_terminal(cursor, first, 0U);
}

void test_truncated_payload_after_frame() {
  constexpr std::array input{
      as_byte(0x00), as_byte(0x01), as_byte(0x41), as_byte(0x00), as_byte(0x02), as_byte(0x42),
  };
  feedforge::binary_file_cursor cursor{std::span<const std::byte>{input}};

  const auto frame = cursor.next();
  FEEDFORGE_CHECK(frame.status == feedforge::frame_status::frame);
  FEEDFORGE_CHECK(frame.error == feedforge::framing_errc::none);
  FEEDFORGE_CHECK(frame.offset == 0U);
  FEEDFORGE_CHECK(frame.frame.file_offset == 0U);
  FEEDFORGE_CHECK(frame.frame.ordinal == 0U);
  FEEDFORGE_CHECK(cursor.consumed() == 3U);
  FEEDFORGE_CHECK(cursor.remaining().empty());

  const auto error = cursor.next();
  FEEDFORGE_CHECK(error.status == feedforge::frame_status::error);
  FEEDFORGE_CHECK(error.error == feedforge::framing_errc::truncated_payload);
  FEEDFORGE_CHECK(error.offset == 3U);
  assert_sticky_terminal(cursor, error, 3U);
}

void test_maximum_payload_length() {
  std::array<std::byte, 65'537> input{};
  input[0] = as_byte(0xff);
  input[1] = as_byte(0xff);
  input[2] = as_byte(0x11);
  input.back() = as_byte(0x22);

  feedforge::binary_file_cursor cursor{std::span<const std::byte>{input}};
  const auto frame = cursor.next();

  FEEDFORGE_CHECK(frame.status == feedforge::frame_status::frame);
  FEEDFORGE_CHECK(frame.error == feedforge::framing_errc::none);
  FEEDFORGE_CHECK(frame.offset == 0U);
  FEEDFORGE_CHECK(frame.frame.file_offset == 0U);
  FEEDFORGE_CHECK(frame.frame.ordinal == 0U);
  FEEDFORGE_CHECK(frame.frame.payload.data() == input.data() + 2U);
  FEEDFORGE_CHECK(frame.frame.payload.size() == 65'535U);
  FEEDFORGE_CHECK(frame.frame.payload.front() == as_byte(0x11));
  FEEDFORGE_CHECK(frame.frame.payload.back() == as_byte(0x22));
  FEEDFORGE_CHECK(cursor.consumed() == input.size());
  FEEDFORGE_CHECK(cursor.remaining().empty());

  const auto incomplete = cursor.next();
  FEEDFORGE_CHECK(incomplete.status == feedforge::frame_status::incomplete);
  FEEDFORGE_CHECK(incomplete.error == feedforge::framing_errc::none);
  FEEDFORGE_CHECK(incomplete.offset == input.size());
  assert_sticky_terminal(cursor, incomplete, input.size());
  FEEDFORGE_CHECK(frame.frame.payload.front() == as_byte(0x11));
  FEEDFORGE_CHECK(frame.frame.payload.back() == as_byte(0x22));
}

} // namespace

int main() {
  test_empty_incomplete_file();
  test_empty_complete_file();
  test_one_frame_incomplete_session();
  test_multiple_frames_and_complete_session();
  test_remaining_after_end_marker();
  test_truncated_length_prefix();
  test_truncated_length_prefix_after_frame();
  test_truncated_payload();
  test_truncated_payload_after_frame();
  test_maximum_payload_length();
}

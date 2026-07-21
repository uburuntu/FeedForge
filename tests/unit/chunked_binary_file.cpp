#include "test_support.hpp"

#include <feedforge/framing/binary_file.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

namespace {

constexpr std::byte byte(unsigned int value) noexcept {
  return std::byte{static_cast<unsigned char>(value)};
}

static_assert(std::is_same_v<std::underlying_type_t<feedforge::chunk_frame_status>, std::uint8_t>);
static_assert(
    std::is_nothrow_constructible_v<feedforge::chunked_binary_file_cursor, std::span<std::byte>>);
static_assert(noexcept(std::declval<feedforge::chunked_binary_file_cursor&>().next({})));
static_assert(noexcept(std::declval<feedforge::chunked_binary_file_cursor&>().finish()));
static_assert(noexcept(std::declval<const feedforge::chunked_binary_file_cursor&>().consumed()));
static_assert(noexcept(std::declval<const feedforge::chunked_binary_file_cursor&>().received()));

constexpr std::uint64_t maximum_offset = std::numeric_limits<std::uint64_t>::max();
static_assert(feedforge::detail::representable_offset_bytes(maximum_offset - 1U, 2U) == 1U);
static_assert(feedforge::detail::representable_offset_bytes(maximum_offset - 1U, 1U) == 1U);
static_assert(feedforge::detail::representable_offset_bytes(maximum_offset, 1U) == 0U);

void check_one_byte_chunks_and_finish() {
  constexpr std::array input{
      byte(0x00), byte(0x03), byte('A'), byte('B'), byte('C'), byte(0x00), byte(0x00),
  };
  std::array<std::byte, 3U> scratch{};
  feedforge::chunked_binary_file_cursor cursor{scratch};

  std::size_t frames{};
  for (std::size_t index = 0U; index < input.size(); ++index) {
    const auto outcome = cursor.next(std::span<const std::byte>{input}.subspan(index, 1U));
    FEEDFORGE_CHECK(outcome.input_consumed == 1U);
    FEEDFORGE_CHECK(outcome.error == feedforge::framing_errc::none);
    if (outcome.status == feedforge::chunk_frame_status::frame) {
      ++frames;
      FEEDFORGE_CHECK(index == 4U);
      FEEDFORGE_CHECK(outcome.frame.file_offset == 0U);
      FEEDFORGE_CHECK(outcome.frame.ordinal == 0U);
      FEEDFORGE_CHECK(outcome.frame.payload.size() == 3U);
      FEEDFORGE_CHECK(outcome.frame.payload[0U] == byte('A'));
      FEEDFORGE_CHECK(outcome.frame.payload[2U] == byte('C'));
    } else {
      FEEDFORGE_CHECK(outcome.status == feedforge::chunk_frame_status::needs_input);
    }
  }

  FEEDFORGE_CHECK(frames == 1U);
  FEEDFORGE_CHECK(cursor.received() == input.size());
  FEEDFORGE_CHECK(cursor.consumed() == input.size());

  const auto complete = cursor.finish();
  FEEDFORGE_CHECK(complete.status == feedforge::chunk_frame_status::complete);
  FEEDFORGE_CHECK(complete.offset == 5U);
  FEEDFORGE_CHECK(complete.input_consumed == 0U);
  const auto repeated = cursor.finish();
  FEEDFORGE_CHECK(repeated.status == complete.status);
  FEEDFORGE_CHECK(repeated.error == complete.error);
  FEEDFORGE_CHECK(repeated.offset == complete.offset);
  FEEDFORGE_CHECK(cursor.next({}).status == complete.status);
}

void check_multiple_frames_in_one_chunk() {
  constexpr std::array input{
      byte(0x00), byte(0x01), byte('A'),  byte(0x00), byte(0x02),
      byte('B'),  byte('C'),  byte(0x00), byte(0x00),
  };
  std::array<std::byte, 2U> scratch{};
  feedforge::chunked_binary_file_cursor cursor{scratch};
  std::size_t input_position{};
  std::size_t frames{};

  while (input_position != input.size()) {
    const auto outcome = cursor.next(std::span<const std::byte>{input}.subspan(input_position));
    input_position += outcome.input_consumed;
    if (outcome.status == feedforge::chunk_frame_status::frame) {
      FEEDFORGE_CHECK(outcome.frame.ordinal == frames);
      FEEDFORGE_CHECK(outcome.frame.file_offset == (frames == 0U ? 0U : 3U));
      ++frames;
      continue;
    }
    FEEDFORGE_CHECK(outcome.status == feedforge::chunk_frame_status::needs_input);
  }

  FEEDFORGE_CHECK(frames == 2U);
  FEEDFORGE_CHECK(cursor.finish().status == feedforge::chunk_frame_status::complete);
}

void check_finish_classification() {
  std::array<std::byte, 8U> scratch{};

  {
    feedforge::chunked_binary_file_cursor cursor{scratch};
    const auto outcome = cursor.finish();
    FEEDFORGE_CHECK(outcome.status == feedforge::chunk_frame_status::incomplete);
    FEEDFORGE_CHECK(outcome.error == feedforge::framing_errc::none);
    FEEDFORGE_CHECK(outcome.offset == 0U);
  }

  {
    feedforge::chunked_binary_file_cursor cursor{scratch};
    constexpr std::array input{byte(0x00)};
    FEEDFORGE_CHECK(cursor.next(input).status == feedforge::chunk_frame_status::needs_input);
    const auto outcome = cursor.finish();
    FEEDFORGE_CHECK(outcome.status == feedforge::chunk_frame_status::error);
    FEEDFORGE_CHECK(outcome.error == feedforge::framing_errc::truncated_length_prefix);
    FEEDFORGE_CHECK(outcome.offset == 0U);
    FEEDFORGE_CHECK(cursor.consumed() == 0U);
    FEEDFORGE_CHECK(cursor.received() == 1U);
  }

  {
    feedforge::chunked_binary_file_cursor cursor{scratch};
    constexpr std::array input{byte(0x00), byte(0x03), byte('A')};
    FEEDFORGE_CHECK(cursor.next(input).status == feedforge::chunk_frame_status::needs_input);
    const auto outcome = cursor.finish();
    FEEDFORGE_CHECK(outcome.status == feedforge::chunk_frame_status::error);
    FEEDFORGE_CHECK(outcome.error == feedforge::framing_errc::truncated_payload);
    FEEDFORGE_CHECK(outcome.offset == 0U);
    FEEDFORGE_CHECK(cursor.consumed() == 0U);
    FEEDFORGE_CHECK(cursor.received() == input.size());
  }
}

void check_trailing_data_across_chunks() {
  std::array<std::byte, 1U> scratch{};
  feedforge::chunked_binary_file_cursor cursor{scratch};
  constexpr std::array marker{byte(0x00), byte(0x00)};
  constexpr std::array trailing{byte(0x7f)};

  const auto provisional = cursor.next(marker);
  FEEDFORGE_CHECK(provisional.status == feedforge::chunk_frame_status::needs_input);
  FEEDFORGE_CHECK(cursor.consumed() == marker.size());

  const auto error = cursor.next(trailing);
  FEEDFORGE_CHECK(error.status == feedforge::chunk_frame_status::error);
  FEEDFORGE_CHECK(error.error == feedforge::framing_errc::trailing_data_after_end_marker);
  FEEDFORGE_CHECK(error.offset == marker.size());
  FEEDFORGE_CHECK(error.input_consumed == 0U);
  FEEDFORGE_CHECK(cursor.received() == marker.size());
  FEEDFORGE_CHECK(cursor.consumed() == marker.size());
  FEEDFORGE_CHECK(cursor.finish().error == error.error);
}

void check_insufficient_scratch() {
  std::array<std::byte, 2U> scratch{};
  feedforge::chunked_binary_file_cursor cursor{scratch};
  constexpr std::array input{byte(0x00), byte(0x03), byte('A'), byte('B'), byte('C')};

  const auto error = cursor.next(input);
  FEEDFORGE_CHECK(error.status == feedforge::chunk_frame_status::error);
  FEEDFORGE_CHECK(error.error == feedforge::framing_errc::insufficient_scratch);
  FEEDFORGE_CHECK(error.offset == 0U);
  FEEDFORGE_CHECK(error.input_consumed == 2U);
  FEEDFORGE_CHECK(cursor.received() == 2U);
  FEEDFORGE_CHECK(cursor.consumed() == 0U);
  FEEDFORGE_CHECK(cursor.next(input).error == error.error);
  FEEDFORGE_CHECK(cursor.finish().error == error.error);
}

void check_maximum_payload_size() {
  constexpr std::size_t maximum_payload_size{65'535U};
  constexpr std::array prefix{byte(0xff), byte(0xff)};
  std::array<std::byte, maximum_payload_size> payload{};
  std::array<std::byte, maximum_payload_size> scratch{};
  feedforge::chunked_binary_file_cursor cursor{scratch};

  const auto prefix_outcome = cursor.next(prefix);
  FEEDFORGE_CHECK(prefix_outcome.status == feedforge::chunk_frame_status::needs_input);
  FEEDFORGE_CHECK(prefix_outcome.input_consumed == prefix.size());

  const auto frame = cursor.next(payload);
  FEEDFORGE_CHECK(frame.status == feedforge::chunk_frame_status::frame);
  FEEDFORGE_CHECK(frame.input_consumed == maximum_payload_size);
  FEEDFORGE_CHECK(frame.frame.payload.size() == maximum_payload_size);
  FEEDFORGE_CHECK(frame.frame.file_offset == 0U);
  FEEDFORGE_CHECK(frame.frame.ordinal == 0U);
  FEEDFORGE_CHECK(cursor.received() == maximum_payload_size + prefix.size());
  FEEDFORGE_CHECK(cursor.consumed() == maximum_payload_size + prefix.size());

  const auto finished = cursor.finish();
  FEEDFORGE_CHECK(finished.status == feedforge::chunk_frame_status::incomplete);
  FEEDFORGE_CHECK(finished.offset == maximum_payload_size + prefix.size());
}

} // namespace

int main() {
  check_one_byte_chunks_and_finish();
  check_multiple_frames_in_one_chunk();
  check_finish_classification();
  check_trailing_data_across_chunks();
  check_insufficient_scratch();
  check_maximum_payload_size();
}

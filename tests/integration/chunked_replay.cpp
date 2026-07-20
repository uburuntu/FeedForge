#include "test_support.hpp"

#include <feedforge/generated/nasdaq/itch50_all.hpp>
#include <feedforge/generated/nasdaq/itch50_order_events.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

namespace all_messages = feedforge::generated::nasdaq::itch50_all;
namespace order_events = feedforge::generated::nasdaq::itch50_order_events;

template <std::size_t Left, std::size_t Right>
[[nodiscard]] constexpr std::array<std::byte, Left + Right>
concatenate(const std::array<std::byte, Left>& left,
            const std::array<std::byte, Right>& right) noexcept {
  std::array<std::byte, Left + Right> result{};
  for (std::size_t index = 0U; index < Left; ++index) {
    result[index] = left[index];
  }
  for (std::size_t index = 0U; index < Right; ++index) {
    result[Left + index] = right[index];
  }
  return result;
}

template <std::size_t Size>
[[nodiscard]] constexpr std::array<std::byte, Size + 2U>
frame(const std::array<std::byte, Size>& payload) noexcept {
  std::array<std::byte, Size + 2U> result{};
  result[0U] = std::byte{static_cast<unsigned char>((Size >> 8U) & 0xffU)};
  result[1U] = std::byte{static_cast<unsigned char>(Size & 0xffU)};
  for (std::size_t index = 0U; index < Size; ++index) {
    result[index + 2U] = payload[index];
  }
  return result;
}

constexpr auto add_order_payload = [] {
  std::array<std::byte, 36U> result{};
  result[0U] = std::byte{'A'};
  return result;
}();

constexpr auto system_event_payload = [] {
  std::array<std::byte, 12U> result{};
  result[0U] = std::byte{'S'};
  return result;
}();

constexpr auto add_order_frame = frame(add_order_payload);
constexpr auto system_event_frame = frame(system_event_payload);
constexpr std::array end_marker{std::byte{0U}, std::byte{0U}};
constexpr auto selected_unselected = concatenate(add_order_frame, system_event_frame);
constexpr auto three_frames = concatenate(selected_unselected, add_order_frame);
constexpr auto complete_file = concatenate(three_frames, end_marker);
constexpr auto trailing_file = concatenate(complete_file, std::array{std::byte{0x7fU}});
constexpr auto truncated_prefix_file = concatenate(add_order_frame, std::array{std::byte{0U}});
constexpr auto truncated_payload_file =
    concatenate(add_order_frame, std::array{std::byte{0U}, std::byte{3U}, std::byte{'A'}});
constexpr auto malformed_known_file =
    concatenate(std::array{std::byte{0U}, std::byte{1U}, std::byte{'A'}}, end_marker);
constexpr auto unknown_file =
    concatenate(std::array{std::byte{0U}, std::byte{1U}, std::byte{'?'}}, end_marker);

struct recording_sink {
  std::array<std::byte, 8U> discriminators{};
  std::size_t calls{};
  bool stop_on_first{};

  template <class Event> feedforge::flow operator()(const Event&) noexcept {
    discriminators[calls] = Event::source_discriminator;
    ++calls;
    return stop_on_first ? feedforge::flow::stop : feedforge::flow::continue_;
  }
};

struct run_result {
  feedforge::replay_summary summary;
  recording_sink sink;
};

void check_decode_outcome(const feedforge::decode_outcome& actual,
                          const feedforge::decode_outcome& expected) {
  FEEDFORGE_CHECK(actual.status == expected.status);
  FEEDFORGE_CHECK(actual.message_type == expected.message_type);
  FEEDFORGE_CHECK(actual.expected_size == expected.expected_size);
  FEEDFORGE_CHECK(actual.actual_size == expected.actual_size);
}

void check_equivalent(const run_result& actual, const run_result& expected) {
  FEEDFORGE_CHECK(actual.summary.status == expected.summary.status);
  FEEDFORGE_CHECK(actual.summary.frames_seen == expected.summary.frames_seen);
  FEEDFORGE_CHECK(actual.summary.events_emitted == expected.summary.events_emitted);
  FEEDFORGE_CHECK(actual.summary.known_messages_skipped == expected.summary.known_messages_skipped);
  FEEDFORGE_CHECK(actual.summary.unknown_messages_skipped ==
                  expected.summary.unknown_messages_skipped);
  FEEDFORGE_CHECK(actual.summary.bytes_consumed == expected.summary.bytes_consumed);
  FEEDFORGE_CHECK(actual.summary.error_offset == expected.summary.error_offset);
  FEEDFORGE_CHECK(actual.summary.framing_error == expected.summary.framing_error);
  check_decode_outcome(actual.summary.decode_error, expected.summary.decode_error);
  FEEDFORGE_CHECK(actual.sink.calls == expected.sink.calls);
  for (std::size_t index = 0U; index < expected.sink.calls; ++index) {
    FEEDFORGE_CHECK(actual.sink.discriminators[index] == expected.sink.discriminators[index]);
  }
}

template <std::size_t Size, class OneShot>
[[nodiscard]] run_result run_one_shot(const std::array<std::byte, Size>& input, OneShot one_shot,
                                      bool stop_on_first) {
  recording_sink sink{{}, 0U, stop_on_first};
  return run_result{one_shot(std::span<const std::byte>{input}, sink), sink};
}

template <template <class> class Replayer, std::size_t Size>
[[nodiscard]] run_result run_at_split(const std::array<std::byte, Size>& input, std::size_t split,
                                      bool stop_on_first) {
  std::array<std::byte, 64U> scratch{};
  recording_sink sink{{}, 0U, stop_on_first};
  Replayer<recording_sink> replay{scratch, sink};
  const auto bytes = std::span<const std::byte>{input};
  static_cast<void>(replay.push(bytes.first(split)));
  static_cast<void>(replay.push(bytes.subspan(split)));
  const auto summary = replay.finish();
  return run_result{summary, sink};
}

template <template <class> class Replayer, std::size_t Size>
[[nodiscard]] run_result run_one_byte_chunks(const std::array<std::byte, Size>& input,
                                             bool stop_on_first) {
  std::array<std::byte, 64U> scratch{};
  recording_sink sink{{}, 0U, stop_on_first};
  Replayer<recording_sink> replay{scratch, sink};
  const auto bytes = std::span<const std::byte>{input};
  for (std::size_t index = 0U; index < bytes.size(); ++index) {
    static_cast<void>(replay.push(bytes.subspan(index, 1U)));
  }
  return run_result{replay.finish(), sink};
}

template <template <class> class Replayer, std::size_t Size>
[[nodiscard]] run_result run_partitioned(const std::array<std::byte, Size>& input,
                                         std::uint32_t seed, bool stop_on_first) {
  std::array<std::byte, 64U> scratch{};
  recording_sink sink{{}, 0U, stop_on_first};
  Replayer<recording_sink> replay{scratch, sink};
  const auto bytes = std::span<const std::byte>{input};
  std::size_t position{};
  while (position != bytes.size()) {
    seed = seed * 1664525U + 1013904223U;
    if ((seed & 3U) == 0U) {
      static_cast<void>(replay.push({}));
    }
    const std::size_t proposed = 1U + static_cast<std::size_t>(seed % 11U);
    const std::size_t remaining = bytes.size() - position;
    const std::size_t count = std::min(proposed, remaining);
    static_cast<void>(replay.push(bytes.subspan(position, count)));
    position += count;
  }
  return run_result{replay.finish(), sink};
}

template <template <class> class Replayer, std::size_t Size, class OneShot>
void check_all_partitions(const std::array<std::byte, Size>& input, OneShot one_shot,
                          bool stop_on_first = false) {
  const run_result expected = run_one_shot(input, one_shot, stop_on_first);
  for (std::size_t split = 0U; split <= input.size(); ++split) {
    check_equivalent(run_at_split<Replayer>(input, split, stop_on_first), expected);
  }
  check_equivalent(run_one_byte_chunks<Replayer>(input, stop_on_first), expected);
  for (std::uint32_t seed = 1U; seed <= 256U; ++seed) {
    check_equivalent(run_partitioned<Replayer>(input, seed, stop_on_first), expected);
  }
}

void check_order_event_equivalence() {
  const auto one_shot = [](std::span<const std::byte> input, recording_sink& sink) noexcept {
    return order_events::replay_binary_file(input, sink);
  };

  check_all_partitions<order_events::chunked_replayer>(complete_file, one_shot);
  check_all_partitions<order_events::chunked_replayer>(three_frames, one_shot);
  check_all_partitions<order_events::chunked_replayer>(trailing_file, one_shot);
  check_all_partitions<order_events::chunked_replayer>(truncated_prefix_file, one_shot);
  check_all_partitions<order_events::chunked_replayer>(truncated_payload_file, one_shot);
  check_all_partitions<order_events::chunked_replayer>(malformed_known_file, one_shot);
  check_all_partitions<order_events::chunked_replayer>(unknown_file, one_shot);
  check_all_partitions<order_events::chunked_replayer>(complete_file, one_shot, true);
}

void check_all_message_equivalence() {
  const auto one_shot = [](std::span<const std::byte> input, recording_sink& sink) noexcept {
    return all_messages::replay_binary_file(input, sink);
  };
  check_all_partitions<all_messages::chunked_replayer>(complete_file, one_shot);
  check_all_partitions<all_messages::chunked_replayer>(malformed_known_file, one_shot);
  check_all_partitions<all_messages::chunked_replayer>(unknown_file, one_shot);
}

void check_completion_and_stickiness() {
  std::array<std::byte, 64U> scratch{};
  recording_sink sink{};
  order_events::chunked_replayer<recording_sink> replay{scratch, sink};

  const auto pushed = replay.push(complete_file);
  FEEDFORGE_CHECK(pushed.status == feedforge::replay_status::incomplete);
  FEEDFORGE_CHECK(pushed.frames_seen == 3U);
  FEEDFORGE_CHECK(pushed.events_emitted == 2U);
  FEEDFORGE_CHECK(pushed.known_messages_skipped == 1U);
  FEEDFORGE_CHECK(pushed.bytes_consumed == complete_file.size());
  FEEDFORGE_CHECK(replay.summary().frames_seen == pushed.frames_seen);

  const auto complete = replay.finish();
  FEEDFORGE_CHECK(complete.status == feedforge::replay_status::complete);
  const auto after_finish = replay.push(std::array{std::byte{0x7fU}});
  check_equivalent(run_result{after_finish, sink}, run_result{complete, sink});
  FEEDFORGE_CHECK(replay.finish().status == feedforge::replay_status::complete);
}

void check_insufficient_scratch() {
  std::array<std::byte, 8U> scratch{};
  recording_sink sink{};
  order_events::chunked_replayer<recording_sink> replay{scratch, sink};

  const auto error = replay.push(add_order_frame);
  FEEDFORGE_CHECK(error.status == feedforge::replay_status::framing_error);
  FEEDFORGE_CHECK(error.framing_error == feedforge::framing_errc::insufficient_scratch);
  FEEDFORGE_CHECK(error.frames_seen == 0U);
  FEEDFORGE_CHECK(error.bytes_consumed == 0U);
  FEEDFORGE_CHECK(error.error_offset == 0U);
  FEEDFORGE_CHECK(sink.calls == 0U);
  FEEDFORGE_CHECK(replay.finish().framing_error == error.framing_error);
}

} // namespace

int main() {
  check_order_event_equivalence();
  check_all_message_equivalence();
  check_completion_and_stickiness();
  check_insufficient_scratch();
}

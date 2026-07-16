#include "test_support.hpp"

#include <feedforge/generated/nasdaq/itch50_all.hpp>
#include <feedforge/generated/nasdaq/itch50_order_events.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>

namespace {

namespace all_messages = feedforge::generated::nasdaq::itch50_all;
namespace order_events = feedforge::generated::nasdaq::itch50_order_events;

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

template <std::size_t Size>
[[nodiscard]] constexpr std::array<std::byte, Size + 2U>
with_end_marker(const std::array<std::byte, Size>& input) noexcept {
  std::array<std::byte, Size + 2U> result{};
  for (std::size_t index = 0U; index < Size; ++index) {
    result[index] = input[index];
  }
  return result;
}

constexpr auto add_order_payload = [] {
  std::array<std::byte, 36U> payload{};
  payload[0U] = std::byte{'A'};
  return payload;
}();

constexpr auto system_event_payload = [] {
  std::array<std::byte, 12U> payload{};
  payload[0U] = std::byte{'S'};
  return payload;
}();

constexpr auto add_order_frame = frame(add_order_payload);
constexpr auto add_order_complete = with_end_marker(add_order_frame);

struct observing_noop_sink {
  std::size_t calls{};
  bool stop{};

  template <class Event> feedforge::flow operator()(const Event&) noexcept {
    ++calls;
    return stop ? feedforge::flow::stop : feedforge::flow::continue_;
  }
};

static_assert(all_messages::sink_for_all_selected_events<observing_noop_sink>);
static_assert(order_events::sink_for_all_selected_events<observing_noop_sink>);
static_assert(noexcept(all_messages::decoder{}.decode_one(std::span<const std::byte>{},
                                                          std::declval<observing_noop_sink&>())));
static_assert(noexcept(all_messages::replay_binary_file(std::span<const std::byte>{},
                                                        std::declval<observing_noop_sink&>())));

void check_outcome(const feedforge::decode_outcome& outcome, const feedforge::decode_status status,
                   const std::byte message_type, const std::uint16_t expected_size,
                   const std::size_t actual_size) {
  FEEDFORGE_CHECK(outcome.status == status);
  FEEDFORGE_CHECK(outcome.message_type == message_type);
  FEEDFORGE_CHECK(outcome.expected_size == expected_size);
  FEEDFORGE_CHECK(outcome.actual_size == actual_size);
}

void check_default_error_fields(const feedforge::replay_summary& summary) {
  FEEDFORGE_CHECK(summary.error_offset == 0U);
  FEEDFORGE_CHECK(summary.framing_error == feedforge::framing_errc::none);
  FEEDFORGE_CHECK(summary.decode_error.status == feedforge::decode_status::emitted);
  FEEDFORGE_CHECK(summary.decode_error.message_type == std::byte{0U});
  FEEDFORGE_CHECK(summary.decode_error.expected_size == 0U);
  FEEDFORGE_CHECK(summary.decode_error.actual_size == 0U);
}

void check_decode_statuses() {
  all_messages::decoder all_decoder;
  order_events::decoder projected_decoder;

  observing_noop_sink emitted_sink;
  const auto emitted = all_decoder.decode_one(add_order_payload, emitted_sink);
  check_outcome(emitted, feedforge::decode_status::emitted, std::byte{'A'}, 36U, 36U);
  FEEDFORGE_CHECK(!emitted.is_error() && !emitted.is_terminal());
  FEEDFORGE_CHECK(emitted_sink.calls == 1U);

  observing_noop_sink skipped_sink;
  const auto skipped = projected_decoder.decode_one(system_event_payload, skipped_sink);
  check_outcome(skipped, feedforge::decode_status::known_unselected_skipped, std::byte{'S'}, 12U,
                12U);
  FEEDFORGE_CHECK(!skipped.is_error() && !skipped.is_terminal());
  FEEDFORGE_CHECK(skipped_sink.calls == 0U);

  observing_noop_sink stopped_sink{0U, true};
  const auto stopped = projected_decoder.decode_one(add_order_payload, stopped_sink);
  check_outcome(stopped, feedforge::decode_status::stopped, std::byte{'A'}, 36U, 36U);
  FEEDFORGE_CHECK(!stopped.is_error() && stopped.is_terminal());
  FEEDFORGE_CHECK(stopped_sink.calls == 1U);

  observing_noop_sink error_sink;
  const auto empty = all_decoder.decode_one(std::span<const std::byte>{}, error_sink);
  check_outcome(empty, feedforge::decode_status::empty_payload, std::byte{0U}, 0U, 0U);
  FEEDFORGE_CHECK(empty.is_error() && empty.is_terminal());

  constexpr std::array unknown_payload{std::byte{'?'}};
  const auto unknown = all_decoder.decode_one(unknown_payload, error_sink);
  check_outcome(unknown, feedforge::decode_status::unknown_message_type, std::byte{'?'}, 0U, 1U);
  FEEDFORGE_CHECK(unknown.is_error() && unknown.is_terminal());

  const auto malformed_selected =
      all_decoder.decode_one(std::span{add_order_payload}.first<35U>(), error_sink);
  check_outcome(malformed_selected, feedforge::decode_status::invalid_message_size, std::byte{'A'},
                36U, 35U);
  FEEDFORGE_CHECK(malformed_selected.is_error() && malformed_selected.is_terminal());

  const auto malformed_unselected =
      projected_decoder.decode_one(std::span{system_event_payload}.first<11U>(), error_sink);
  check_outcome(malformed_unselected, feedforge::decode_status::invalid_message_size,
                std::byte{'S'}, 12U, 11U);
  FEEDFORGE_CHECK(error_sink.calls == 0U);
}

void check_natural_replay_statuses() {
  observing_noop_sink sink;

  const auto incomplete = all_messages::replay_binary_file(std::span<const std::byte>{}, sink);
  FEEDFORGE_CHECK(incomplete.status == feedforge::replay_status::incomplete);
  FEEDFORGE_CHECK(incomplete.frames_seen == 0U && incomplete.events_emitted == 0U);
  FEEDFORGE_CHECK(incomplete.bytes_consumed == 0U);
  check_default_error_fields(incomplete);

  constexpr std::array empty_complete{std::byte{0U}, std::byte{0U}};
  const auto complete = all_messages::replay_binary_file(empty_complete, sink);
  FEEDFORGE_CHECK(complete.status == feedforge::replay_status::complete);
  FEEDFORGE_CHECK(complete.frames_seen == 0U && complete.events_emitted == 0U);
  FEEDFORGE_CHECK(complete.bytes_consumed == empty_complete.size());
  check_default_error_fields(complete);

  const auto emitted = all_messages::replay_binary_file(add_order_complete, sink);
  FEEDFORGE_CHECK(emitted.status == feedforge::replay_status::complete);
  FEEDFORGE_CHECK(emitted.frames_seen == 1U && emitted.events_emitted == 1U);
  FEEDFORGE_CHECK(emitted.bytes_consumed == add_order_complete.size());
  check_default_error_fields(emitted);
}

void check_framing_errors() {
  observing_noop_sink sink;

  constexpr std::array truncated_prefix{std::byte{0x12U}};
  const auto prefix = all_messages::replay_binary_file(truncated_prefix, sink);
  FEEDFORGE_CHECK(prefix.status == feedforge::replay_status::framing_error);
  FEEDFORGE_CHECK(prefix.framing_error == feedforge::framing_errc::truncated_length_prefix);
  FEEDFORGE_CHECK(prefix.frames_seen == 0U && prefix.events_emitted == 0U);
  FEEDFORGE_CHECK(prefix.bytes_consumed == 0U && prefix.error_offset == 0U);

  constexpr std::array truncated_payload{std::byte{0U}, std::byte{3U}, std::byte{'A'},
                                         std::byte{0U}};
  const auto payload = all_messages::replay_binary_file(truncated_payload, sink);
  FEEDFORGE_CHECK(payload.status == feedforge::replay_status::framing_error);
  FEEDFORGE_CHECK(payload.framing_error == feedforge::framing_errc::truncated_payload);
  FEEDFORGE_CHECK(payload.frames_seen == 0U && payload.events_emitted == 0U);
  FEEDFORGE_CHECK(payload.bytes_consumed == 0U && payload.error_offset == 0U);

  constexpr std::array trailing{std::byte{0U}, std::byte{0U}, std::byte{0xdeU}, std::byte{0xadU}};
  const auto after_marker = all_messages::replay_binary_file(trailing, sink);
  FEEDFORGE_CHECK(after_marker.status == feedforge::replay_status::framing_error);
  FEEDFORGE_CHECK(after_marker.framing_error ==
                  feedforge::framing_errc::trailing_data_after_end_marker);
  FEEDFORGE_CHECK(after_marker.frames_seen == 0U && after_marker.events_emitted == 0U);
  FEEDFORGE_CHECK(after_marker.bytes_consumed == 2U && after_marker.error_offset == 2U);
  FEEDFORGE_CHECK(sink.calls == 0U);
}

void check_decode_error_and_stop_replay() {
  observing_noop_sink sink;

  constexpr std::array unknown_then_valid = [] {
    std::array<std::byte, 3U + add_order_frame.size()> result{};
    result[0U] = std::byte{0U};
    result[1U] = std::byte{1U};
    result[2U] = std::byte{'?'};
    for (std::size_t index = 0U; index < add_order_frame.size(); ++index) {
      result[index + 3U] = add_order_frame[index];
    }
    return result;
  }();
  const auto unknown = all_messages::replay_binary_file(unknown_then_valid, sink);
  FEEDFORGE_CHECK(unknown.status == feedforge::replay_status::decode_error);
  FEEDFORGE_CHECK(unknown.frames_seen == 1U && unknown.events_emitted == 0U);
  FEEDFORGE_CHECK(unknown.bytes_consumed == 3U && unknown.error_offset == 2U);
  check_outcome(unknown.decode_error, feedforge::decode_status::unknown_message_type,
                std::byte{'?'}, 0U, 1U);
  FEEDFORGE_CHECK(unknown.framing_error == feedforge::framing_errc::none);
  FEEDFORGE_CHECK(sink.calls == 0U);

  constexpr std::array malformed_known{std::byte{0U}, std::byte{1U}, std::byte{'A'}};
  const auto malformed = all_messages::replay_binary_file(malformed_known, sink);
  FEEDFORGE_CHECK(malformed.status == feedforge::replay_status::decode_error);
  FEEDFORGE_CHECK(malformed.frames_seen == 1U && malformed.events_emitted == 0U);
  FEEDFORGE_CHECK(malformed.bytes_consumed == malformed_known.size());
  FEEDFORGE_CHECK(malformed.error_offset == 2U);
  check_outcome(malformed.decode_error, feedforge::decode_status::invalid_message_size,
                std::byte{'A'}, 36U, 1U);
  FEEDFORGE_CHECK(sink.calls == 0U);

  observing_noop_sink stopped_sink{0U, true};
  const auto stopped = all_messages::replay_binary_file(add_order_frame, stopped_sink);
  FEEDFORGE_CHECK(stopped.status == feedforge::replay_status::stopped);
  FEEDFORGE_CHECK(stopped.frames_seen == 1U && stopped.events_emitted == 1U);
  FEEDFORGE_CHECK(stopped.known_messages_skipped == 0U);
  FEEDFORGE_CHECK(stopped.unknown_messages_skipped == 0U);
  FEEDFORGE_CHECK(stopped.bytes_consumed == add_order_frame.size());
  FEEDFORGE_CHECK(stopped_sink.calls == 1U);
  check_default_error_fields(stopped);
}

} // namespace

int main() {
  check_decode_statuses();
  check_natural_replay_statuses();
  check_framing_errors();
  check_decode_error_and_stop_replay();
}

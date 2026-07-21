#include "reference/itch50_differential.hpp"

#include <feedforge/framing/binary_file.hpp>
#include <feedforge/generated/nasdaq/itch50_all.hpp>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace all_messages = feedforge::generated::nasdaq::itch50_all;
namespace reference = feedforge_reference::itch50;

int failures = 0;

void check(const bool condition, const std::string_view description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    ++failures;
  }
}

[[nodiscard]] std::string bundle_path(const std::string_view relative) {
  return std::string{FEEDFORGE_CONFORMANCE_BUNDLE_DIR} + '/' + std::string{relative};
}

[[nodiscard]] std::vector<std::byte> read_binary(const std::string_view relative) {
  const std::string path = bundle_path(relative);
  std::ifstream input{path, std::ios::binary};
  check(static_cast<bool>(input), path);
  std::vector<std::byte> result;
  char value{};
  while (input.get(value)) {
    result.push_back(std::byte{static_cast<unsigned char>(value)});
  }
  check(input.eof(), path);
  return result;
}

[[nodiscard]] std::string fixture_stem(const std::size_t index, const std::string_view name) {
  const std::size_t ordinal = index + 1U;
  return (ordinal < 10U ? "0" : "") + std::to_string(ordinal) + '_' + std::string{name};
}

struct counting_sink {
  template <class Event> feedforge::flow operator()(const Event&) noexcept {
    ++calls;
    return feedforge::flow::continue_;
  }

  std::size_t calls{};
};

static_assert(all_messages::sink_for_all_selected_events<counting_sink>);

void test_payloads() {
  for (std::size_t index = 0U; index < reference::message_layouts.size(); ++index) {
    const reference::message_layout& layout = reference::message_layouts[index];
    const std::string stem = fixture_stem(index, layout.name);
    const std::vector<std::byte> payload = read_binary("payloads/" + stem + ".bin");
    const reference::decode_outcome expected = reference::classify(payload);
    check(expected.status == reference::decode_status::emitted, stem + " oracle classification");
    check(expected.message_type == layout.discriminator && expected.expected_size == layout.size &&
              expected.actual_size == layout.size,
          stem + " oracle metadata");
    check(reference::matches_generated(payload), stem + " differential decode");

    for (const std::string_view variant : {"size-minus-one", "size-plus-one"}) {
      const std::string case_name = stem + '.' + std::string{variant};
      const std::vector<std::byte> negative =
          read_binary("negative/payloads/" + case_name + ".bin");
      check(reference::classify(negative).status == reference::decode_status::invalid_message_size,
            case_name + " oracle classification");
      check(reference::matches_generated(negative), case_name + " differential decode");
    }
  }

  const std::vector<std::byte> empty = read_binary("negative/payloads/empty-payload.bin");
  check(reference::classify(empty).status == reference::decode_status::empty_payload,
        "empty payload oracle classification");
  check(reference::matches_generated(empty), "empty payload differential decode");

  const std::vector<std::byte> unknown = read_binary("negative/payloads/unknown-message-type.bin");
  check(reference::classify(unknown).status == reference::decode_status::unknown_message_type,
        "unknown payload oracle classification");
  check(reference::matches_generated(unknown), "unknown payload differential decode");
}

void test_aggregate_stream() {
  const std::vector<std::byte> stream = read_binary("streams/all-messages.binaryfile");
  feedforge::binary_file_cursor cursor{stream};
  std::size_t frames = 0U;
  while (true) {
    const feedforge::frame_outcome outcome = cursor.next();
    if (outcome.status == feedforge::frame_status::frame) {
      check(outcome.frame.ordinal == frames, "aggregate frame ordinal");
      check(reference::matches_generated(outcome.frame.payload),
            "aggregate frame differential decode");
      ++frames;
      continue;
    }
    check(outcome.status == feedforge::frame_status::complete, "aggregate cursor completion");
    check(outcome.error == feedforge::framing_errc::none, "aggregate cursor framing error");
    break;
  }
  check(frames == reference::message_layouts.size(), "aggregate frame count");
  check(cursor.consumed() == stream.size(), "aggregate cursor consumption");
  check(cursor.remaining().empty(), "aggregate cursor trailing bytes");

  counting_sink sink;
  const feedforge::replay_summary summary = all_messages::replay_binary_file(stream, sink);
  check(summary.status == feedforge::replay_status::complete, "aggregate replay completion");
  check(summary.frames_seen == 23U && summary.events_emitted == 23U &&
            summary.known_messages_skipped == 0U && summary.unknown_messages_skipped == 0U,
        "aggregate replay counters");
  check(summary.bytes_consumed == stream.size(), "aggregate replay consumption");
  check(sink.calls == 23U, "aggregate replay sink calls");
}

void test_framing_cases() {
  {
    const std::vector<std::byte> data = read_binary("negative/framing/complete-empty.binaryfile");
    counting_sink sink;
    const feedforge::replay_summary summary = all_messages::replay_binary_file(data, sink);
    check(summary.status == feedforge::replay_status::complete && summary.frames_seen == 0U &&
              summary.bytes_consumed == 2U && sink.calls == 0U,
          "complete empty BinaryFILE");
  }
  {
    const std::vector<std::byte> data =
        read_binary("negative/framing/incomplete-all-messages.binaryfile");
    counting_sink sink;
    const feedforge::replay_summary summary = all_messages::replay_binary_file(data, sink);
    check(summary.status == feedforge::replay_status::incomplete && summary.frames_seen == 23U &&
              summary.events_emitted == 23U && summary.bytes_consumed == data.size() &&
              sink.calls == 23U,
          "incomplete all-message BinaryFILE");
  }
  {
    const std::vector<std::byte> data =
        read_binary("negative/framing/truncated-length-prefix.binaryfile");
    counting_sink sink;
    const feedforge::replay_summary summary = all_messages::replay_binary_file(data, sink);
    check(summary.status == feedforge::replay_status::framing_error &&
              summary.framing_error == feedforge::framing_errc::truncated_length_prefix &&
              summary.frames_seen == 0U && summary.bytes_consumed == 0U &&
              summary.error_offset == 0U,
          "truncated length prefix BinaryFILE");
  }
  {
    const std::vector<std::byte> data =
        read_binary("negative/framing/truncated-payload.binaryfile");
    counting_sink sink;
    const feedforge::replay_summary summary = all_messages::replay_binary_file(data, sink);
    check(summary.status == feedforge::replay_status::framing_error &&
              summary.framing_error == feedforge::framing_errc::truncated_payload &&
              summary.frames_seen == 0U && summary.bytes_consumed == 0U &&
              summary.error_offset == 0U,
          "truncated payload BinaryFILE");
  }
  {
    const std::vector<std::byte> data =
        read_binary("negative/framing/trailing-data-after-end-marker.binaryfile");
    counting_sink sink;
    const feedforge::replay_summary summary = all_messages::replay_binary_file(data, sink);
    check(summary.status == feedforge::replay_status::framing_error &&
              summary.framing_error == feedforge::framing_errc::trailing_data_after_end_marker &&
              summary.frames_seen == 0U && summary.bytes_consumed == 2U &&
              summary.error_offset == 2U,
          "trailing data BinaryFILE");
  }
  {
    const std::vector<std::byte> data =
        read_binary("negative/framing/unknown-message-type.binaryfile");
    counting_sink sink;
    const feedforge::replay_summary summary = all_messages::replay_binary_file(data, sink);
    check(summary.status == feedforge::replay_status::decode_error &&
              summary.decode_error.status == feedforge::decode_status::unknown_message_type &&
              summary.frames_seen == 1U && summary.bytes_consumed == 3U &&
              summary.error_offset == 2U && sink.calls == 0U,
          "unknown message BinaryFILE");
  }
  {
    const std::vector<std::byte> data =
        read_binary("negative/framing/invalid-message-size.binaryfile");
    counting_sink sink;
    const feedforge::replay_summary summary = all_messages::replay_binary_file(data, sink);
    check(summary.status == feedforge::replay_status::decode_error &&
              summary.decode_error.status == feedforge::decode_status::invalid_message_size &&
              summary.decode_error.expected_size == 12U &&
              summary.decode_error.actual_size == 11U && summary.frames_seen == 1U &&
              summary.bytes_consumed == 13U && summary.error_offset == 2U && sink.calls == 0U,
          "invalid message size BinaryFILE");
  }
}

} // namespace

int main() {
  test_payloads();
  test_aggregate_stream();
  test_framing_cases();
  return failures == 0 ? 0 : 1;
}

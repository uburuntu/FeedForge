#include "synthetic_pipeline.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

namespace generated = feedforge::generated::test_projection;

int failures = 0;

void check(const bool condition, const std::string_view message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

struct counting_implementation {
  static constexpr std::string_view variant_id{"counting_test.v1"};
  static inline std::size_t loads = 0U;

  template <std::size_t Width>
    requires feedforge::wire::supported_unsigned_width<Width>
  [[nodiscard]] static std::uint64_t load_unsigned(std::byte const* first) noexcept {
    ++loads;
    return feedforge::profile::portable_checked::load_unsigned<Width>(first);
  }
};

static_assert(feedforge::decoder_implementation<counting_implementation>);
static_assert(std::is_same_v<generated::decoder,
                             generated::basic_decoder<feedforge::profile::portable_checked>>);

struct recording_sink {
  std::size_t add_calls{};
  std::size_t update_calls{};
  generated::add_order last_add{};
  generated::order_update last_update{};
  bool stop_on_add{};

  feedforge::flow operator()(const generated::add_order& event) noexcept {
    ++add_calls;
    last_add = event;
    return stop_on_add ? feedforge::flow::stop : feedforge::flow::continue_;
  }

  feedforge::flow operator()(const generated::order_update& event) noexcept {
    ++update_calls;
    last_update = event;
    return feedforge::flow::continue_;
  }
};

struct missing_sink {
  feedforge::flow operator()(const generated::add_order&) noexcept;
};

struct throwing_sink {
  feedforge::flow operator()(const generated::add_order&) noexcept;
  feedforge::flow operator()(const generated::order_update&);
};

static_assert(generated::sink_for_all_selected_events<recording_sink>);
static_assert(!generated::sink_for_all_selected_events<missing_sink>);
static_assert(!generated::sink_for_all_selected_events<throwing_sink>);

constexpr std::array add_payload{
    std::byte{'A'},  std::byte{0x12}, std::byte{0x34}, std::byte{0x00}, std::byte{0x01},
    std::byte{0xe2}, std::byte{0x40}, std::byte{0xff}, std::byte{0x00},
};
constexpr std::array update_payload{
    std::byte{'U'},
    std::byte{0x01},
    std::byte{0x02},
    std::byte{'Z'},
};
constexpr std::array heartbeat_payload{
    std::byte{'H'},
    std::byte{'Y'},
};

void reset(recording_sink& sink) {
  counting_implementation::loads = 0U;
  sink = recording_sink{};
}

void check_outcome(const feedforge::decode_outcome outcome, const feedforge::decode_status status,
                   const std::byte message_type, const std::uint16_t expected_size,
                   const std::size_t actual_size, const std::string_view description) {
  check(outcome.status == status, std::string(description) + " status");
  check(outcome.message_type == message_type, std::string(description) + " message type");
  check(outcome.expected_size == expected_size, std::string(description) + " expected size");
  check(outcome.actual_size == actual_size, std::string(description) + " actual size");
}

[[nodiscard]] bool same_outcome(const feedforge::decode_outcome& lhs,
                                const feedforge::decode_outcome& rhs) noexcept {
  return lhs.status == rhs.status && lhs.message_type == rhs.message_type &&
         lhs.expected_size == rhs.expected_size && lhs.actual_size == rhs.actual_size;
}

void test_prevalidation_outcomes() {
  generated::basic_decoder<counting_implementation> decoder;
  recording_sink sink;

  reset(sink);
  const auto empty = decoder.decode_one(std::span<const std::byte>{}, sink);
  check_outcome(empty, feedforge::decode_status::empty_payload, std::byte{0x00}, 0U, 0U,
                "empty payload");
  check(empty.is_error() && empty.is_terminal(), "empty payload is an error");
  check(counting_implementation::loads == 0U, "empty payload performs no field loads");
  check(sink.add_calls == 0U && sink.update_calls == 0U, "empty payload does not call sink");

  reset(sink);
  constexpr std::array unknown{std::byte{'?'}};
  const auto unknown_outcome = decoder.decode_one(unknown, sink);
  check_outcome(unknown_outcome, feedforge::decode_status::unknown_message_type, std::byte{'?'}, 0U,
                unknown.size(), "unknown payload");
  check(counting_implementation::loads == 0U, "unknown payload performs no field loads");
  check(sink.add_calls == 0U && sink.update_calls == 0U, "unknown payload does not call sink");

  reset(sink);
  const auto short_selected = decoder.decode_one(std::span{add_payload}.first<8U>(), sink);
  check_outcome(short_selected, feedforge::decode_status::invalid_message_size, std::byte{'A'}, 9U,
                8U, "short selected payload");
  check(counting_implementation::loads == 0U, "short selected payload performs no field loads");

  reset(sink);
  std::array<std::byte, 10U> long_add{};
  for (std::size_t index = 0U; index < add_payload.size(); ++index) {
    long_add[index] = add_payload[index];
  }
  const auto long_selected = decoder.decode_one(long_add, sink);
  check_outcome(long_selected, feedforge::decode_status::invalid_message_size, std::byte{'A'}, 9U,
                10U, "long selected payload");
  check(counting_implementation::loads == 0U, "long selected payload performs no field loads");

  reset(sink);
  const auto known_unselected = decoder.decode_one(heartbeat_payload, sink);
  check_outcome(known_unselected, feedforge::decode_status::known_unselected_skipped,
                std::byte{'H'}, 2U, 2U, "known unselected payload");
  check(!known_unselected.is_error() && !known_unselected.is_terminal(),
        "known unselected payload is a nonterminal skip");
  check(counting_implementation::loads == 0U, "known unselected payload performs no field loads");
  check(sink.add_calls == 0U && sink.update_calls == 0U,
        "known unselected payload does not call sink");

  reset(sink);
  const auto malformed_unselected =
      decoder.decode_one(std::span{heartbeat_payload}.first<1U>(), sink);
  check_outcome(malformed_unselected, feedforge::decode_status::invalid_message_size,
                std::byte{'H'}, 2U, 1U, "malformed unselected payload");
  check(counting_implementation::loads == 0U,
        "malformed unselected payload performs no field loads");
}

void test_selected_events_and_stop() {
  generated::basic_decoder<counting_implementation> decoder;
  recording_sink sink;

  reset(sink);
  const auto add = decoder.decode_one(add_payload, sink);
  check_outcome(add, feedforge::decode_status::emitted, std::byte{'A'}, 9U, add_payload.size(),
                "selected add payload");
  check(counting_implementation::loads == 1U, "selected add loads only projected unsigned field");
  check(sink.add_calls == 1U && sink.update_calls == 0U,
        "selected add calls exactly one sink overload");
  check(sink.last_add.price.raw == 123456U, "selected decimal preserves exact raw value");
  check(std::bit_cast<unsigned char>(sink.last_add.side.raw[0U]) == 0xffU,
        "selected ASCII preserves byte representation");

  reset(sink);
  const auto update = decoder.decode_one(update_payload, sink);
  check_outcome(update, feedforge::decode_status::emitted, std::byte{'U'}, 4U,
                update_payload.size(), "selected update payload");
  check(counting_implementation::loads == 1U,
        "selected update loads only projected unsigned field");
  check(sink.add_calls == 0U && sink.update_calls == 1U,
        "selected update calls exactly one sink overload");
  check(sink.last_update.quantity == 0x0102U, "selected raw unsigned value is exact");
  check(sink.last_update.code.raw[0U] == 'Z', "selected ASCII code is exact");

  reset(sink);
  sink.stop_on_add = true;
  const auto stopped = decoder.decode_one(add_payload, sink);
  check_outcome(stopped, feedforge::decode_status::stopped, std::byte{'A'}, 9U, add_payload.size(),
                "stopped selected payload");
  check(!stopped.is_error() && stopped.is_terminal(),
        "cooperative stop is terminal but not an error");
  check(counting_implementation::loads == 1U,
        "stopped selected payload performs projected loads once");
  check(sink.add_calls == 1U && sink.update_calls == 0U,
        "cooperative stop follows exactly one direct sink call");

  generated::decoder portable_decoder;
  reset(sink);
  const auto portable = portable_decoder.decode_one(update_payload, sink);
  check(portable.status == feedforge::decode_status::emitted,
        "public decoder alias uses portable_checked implementation");
}

template <std::size_t Size>
void check_profile_equivalence(const std::array<std::byte, Size>& payload, const bool stop,
                               const std::string_view description) {
  generated::basic_decoder<counting_implementation> instrumented_decoder;
  generated::decoder portable_decoder;
  recording_sink instrumented_sink;
  recording_sink portable_sink;
  instrumented_sink.stop_on_add = stop;
  portable_sink.stop_on_add = stop;

  counting_implementation::loads = 0U;
  const auto instrumented = instrumented_decoder.decode_one(payload, instrumented_sink);
  const auto portable = portable_decoder.decode_one(payload, portable_sink);

  check(same_outcome(instrumented, portable),
        std::string(description) + " outcome is profile-independent");
  check(instrumented_sink.add_calls == portable_sink.add_calls &&
            instrumented_sink.update_calls == portable_sink.update_calls,
        std::string(description) + " sink ordering is profile-independent");
}

void test_profile_semantics_equivalence() {
  constexpr std::array<std::byte, 0U> empty_payload{};
  constexpr std::array unknown_payload{std::byte{'?'}};
  constexpr std::array short_selected{
      std::byte{'A'},
      std::byte{0x12},
      std::byte{0x34},
  };
  constexpr std::array malformed_unselected{std::byte{'H'}};

  std::array<std::byte, 10U> long_selected{};
  for (std::size_t index = 0U; index < add_payload.size(); ++index) {
    long_selected[index] = add_payload[index];
  }

  check_profile_equivalence(empty_payload, false, "empty");
  check_profile_equivalence(unknown_payload, false, "unknown");
  check_profile_equivalence(short_selected, false, "undersized selected");
  check_profile_equivalence(long_selected, false, "oversized selected");
  check_profile_equivalence(heartbeat_payload, false, "known unselected");
  check_profile_equivalence(malformed_unselected, false, "malformed known unselected");
  check_profile_equivalence(add_payload, false, "selected continue");
  check_profile_equivalence(add_payload, true, "selected stop");
  check_profile_equivalence(update_payload, false, "second selected event");
}

void test_replay_adapter() {
  recording_sink sink;

  constexpr std::array complete_add{
      std::byte{0x00}, std::byte{0x09}, std::byte{'A'},  std::byte{0x12}, std::byte{0x34},
      std::byte{0x00}, std::byte{0x01}, std::byte{0xe2}, std::byte{0x40}, std::byte{0xff},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
  };
  reset(sink);
  const auto complete = generated::replay_binary_file(complete_add, sink);
  check(complete.status == feedforge::replay_status::complete,
        "replay reaches strict complete marker");
  check(complete.frames_seen == 1U && complete.events_emitted == 1U,
        "complete replay counts frame and event");
  check(complete.known_messages_skipped == 0U && complete.unknown_messages_skipped == 0U,
        "complete selected replay has no skips");
  check(complete.bytes_consumed == complete_add.size(), "complete replay consumes marker");
  check(sink.add_calls == 1U && sink.update_calls == 0U,
        "complete replay invokes selected sink once");

  constexpr std::array incomplete_unselected{
      std::byte{0x00},
      std::byte{0x02},
      std::byte{'H'},
      std::byte{'Y'},
  };
  reset(sink);
  const auto incomplete = generated::replay_binary_file(incomplete_unselected, sink);
  check(incomplete.status == feedforge::replay_status::incomplete,
        "replay distinguishes clean incomplete input");
  check(incomplete.frames_seen == 1U && incomplete.known_messages_skipped == 1U &&
            incomplete.events_emitted == 0U,
        "incomplete replay counts known unselected frame");
  check(incomplete.bytes_consumed == incomplete_unselected.size(),
        "incomplete replay reports exact consumed bytes");

  constexpr std::array malformed_known{
      std::byte{0x00},
      std::byte{0x01},
      std::byte{'H'},
  };
  reset(sink);
  const auto decode_error = generated::replay_binary_file(malformed_known, sink);
  check(decode_error.status == feedforge::replay_status::decode_error,
        "replay reports malformed known payload as decode error");
  check(decode_error.frames_seen == 1U && decode_error.bytes_consumed == malformed_known.size(),
        "decode error includes accepted frame bytes");
  check(decode_error.error_offset == 2U &&
            decode_error.decode_error.status == feedforge::decode_status::invalid_message_size,
        "decode error reports payload offset and exact outcome");

  constexpr std::array trailing_after_marker{
      std::byte{0x00},
      std::byte{0x00},
      std::byte{0xff},
  };
  reset(sink);
  const auto trailing = generated::replay_binary_file(trailing_after_marker, sink);
  check(trailing.status == feedforge::replay_status::framing_error,
        "strict replay rejects trailing marker data");
  check(trailing.framing_error == feedforge::framing_errc::trailing_data_after_end_marker &&
            trailing.error_offset == 2U && trailing.bytes_consumed == 2U,
        "trailing-data replay reports exact error and offset");

  constexpr std::array stopped_with_uninspected_tail{
      std::byte{0x00}, std::byte{0x09}, std::byte{'A'},  std::byte{0x12},
      std::byte{0x34}, std::byte{0x00}, std::byte{0x01}, std::byte{0xe2},
      std::byte{0x40}, std::byte{0xff}, std::byte{0x00}, std::byte{0xff},
  };
  reset(sink);
  sink.stop_on_add = true;
  const auto stopped = generated::replay_binary_file(stopped_with_uninspected_tail, sink);
  check(stopped.status == feedforge::replay_status::stopped,
        "replay propagates cooperative sink stop");
  check(stopped.frames_seen == 1U && stopped.events_emitted == 1U && stopped.bytes_consumed == 11U,
        "stopped replay includes stopping frame and ignores later byte");
}

} // namespace

int main() {
  test_prevalidation_outcomes();
  test_selected_events_and_stop();
  test_profile_semantics_equivalence();
  test_replay_adapter();

  if (failures != 0) {
    std::cerr << failures << " generated semantic test(s) failed\n";
    return 1;
  }
  return 0;
}

#include <feedforge/generated/nasdaq/itch50_all.hpp>

#include "fuzz_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

namespace generated = feedforge::generated::nasdaq::itch50_all;

struct observing_noop_sink {
  std::uint64_t calls{};
  std::uint64_t type_digest{1469598103934665603ULL};
  std::byte last_type{};
  bool stop{};

  template <class Event> feedforge::flow operator()(const Event&) noexcept {
    ++calls;
    last_type = Event::source_discriminator;
    type_digest ^= std::to_integer<std::uint64_t>(last_type);
    type_digest *= 1099511628211ULL;
    return stop ? feedforge::flow::stop : feedforge::flow::continue_;
  }
};

static_assert(generated::sink_for_all_selected_events<observing_noop_sink>);

struct observation {
  feedforge::replay_summary summary{};
  std::uint64_t sink_calls{};
  std::uint64_t type_digest{};
  std::byte last_type{};
};

[[nodiscard]] bool same_decode_outcome(const feedforge::decode_outcome& lhs,
                                       const feedforge::decode_outcome& rhs) noexcept {
  return lhs.status == rhs.status && lhs.message_type == rhs.message_type &&
         lhs.expected_size == rhs.expected_size && lhs.actual_size == rhs.actual_size;
}

[[nodiscard]] bool same_summary(const feedforge::replay_summary& lhs,
                                const feedforge::replay_summary& rhs) noexcept {
  return lhs.status == rhs.status && lhs.frames_seen == rhs.frames_seen &&
         lhs.events_emitted == rhs.events_emitted &&
         lhs.known_messages_skipped == rhs.known_messages_skipped &&
         lhs.unknown_messages_skipped == rhs.unknown_messages_skipped &&
         lhs.bytes_consumed == rhs.bytes_consumed && lhs.error_offset == rhs.error_offset &&
         lhs.framing_error == rhs.framing_error &&
         same_decode_outcome(lhs.decode_error, rhs.decode_error);
}

[[nodiscard]] observation replay(const std::span<const std::byte> input, const bool stop) noexcept {
  observing_noop_sink sink{0U, 1469598103934665603ULL, std::byte{0U}, stop};
  const feedforge::replay_summary summary = generated::replay_binary_file(input, sink);
  return observation{summary, sink.calls, sink.type_digest, sink.last_type};
}

[[nodiscard]] observation replay_chunked(const std::span<const std::byte> input, const bool stop,
                                         const bool one_byte_chunks) noexcept {
  std::array<std::byte, 65'535U> scratch;
  observing_noop_sink sink{0U, 1469598103934665603ULL, std::byte{0U}, stop};
  generated::chunked_replayer<observing_noop_sink> replay{scratch, sink};

  std::uint32_t state = 0x9e3779b9U;
  for (const std::byte value : input) {
    state ^= std::to_integer<std::uint32_t>(value) + 0x9e3779b9U + (state << 6U) + (state >> 2U);
  }

  std::size_t position{};
  while (position != input.size()) {
    state = state * 1664525U + 1013904223U;
    if (!one_byte_chunks && (state & 7U) == 0U) {
      static_cast<void>(replay.push({}));
    }
    const std::size_t requested = one_byte_chunks ? 1U : 1U + static_cast<std::size_t>(state % 31U);
    const std::size_t remaining = input.size() - position;
    const std::size_t count = requested < remaining ? requested : remaining;
    static_cast<void>(replay.push(input.subspan(position, count)));
    position += count;
  }

  const feedforge::replay_summary summary = replay.finish();
  return observation{summary, sink.calls, sink.type_digest, sink.last_type};
}

void require_same_observation(const observation& actual, const observation& expected) noexcept {
  using feedforge::fuzz::require;
  require(same_summary(actual.summary, expected.summary));
  require(actual.sink_calls == expected.sink_calls);
  require(actual.type_digest == expected.type_digest);
  require(actual.last_type == expected.last_type);
}

void require_default_error_fields(const feedforge::replay_summary& summary) noexcept {
  using feedforge::fuzz::require;
  require(summary.error_offset == 0U);
  require(summary.framing_error == feedforge::framing_errc::none);
  require(summary.decode_error.status == feedforge::decode_status::emitted);
  require(summary.decode_error.message_type == std::byte{0U});
  require(summary.decode_error.expected_size == 0U);
  require(summary.decode_error.actual_size == 0U);
}

void validate(const std::span<const std::byte> input, const observation& result,
              const bool stop) noexcept {
  using feedforge::fuzz::require;
  const feedforge::replay_summary& summary = result.summary;

  require(summary.bytes_consumed <= input.size());
  require(summary.events_emitted == result.sink_calls);
  require(summary.events_emitted <= summary.frames_seen);
  require(summary.known_messages_skipped == 0U);
  require(summary.unknown_messages_skipped == 0U);

  switch (summary.status) {
  case feedforge::replay_status::complete:
    require(summary.bytes_consumed == input.size());
    require(summary.frames_seen == summary.events_emitted);
    require_default_error_fields(summary);
    break;
  case feedforge::replay_status::incomplete:
    require(summary.bytes_consumed == input.size());
    require(summary.frames_seen == summary.events_emitted);
    require_default_error_fields(summary);
    break;
  case feedforge::replay_status::stopped:
    require(stop);
    require(summary.frames_seen == 1U);
    require(summary.events_emitted == 1U);
    require(result.sink_calls == 1U);
    require_default_error_fields(summary);
    break;
  case feedforge::replay_status::framing_error:
    require(summary.frames_seen == summary.events_emitted);
    require(summary.framing_error != feedforge::framing_errc::none);
    require(summary.decode_error.status == feedforge::decode_status::emitted);
    require(summary.error_offset == summary.bytes_consumed);
    require(summary.error_offset <= input.size());
    if (summary.framing_error == feedforge::framing_errc::trailing_data_after_end_marker) {
      require(summary.bytes_consumed < input.size());
    }
    break;
  case feedforge::replay_status::decode_error:
    require(summary.frames_seen == summary.events_emitted + 1U);
    require(summary.framing_error == feedforge::framing_errc::none);
    require(summary.decode_error.is_error());
    require(summary.error_offset <= summary.bytes_consumed);
    require(result.sink_calls == summary.events_emitted);
    break;
  }
}

void exercise_replay(const std::span<const std::byte> input) noexcept {
  using feedforge::fuzz::require;

  const observation first = replay(input, false);
  const observation second = replay(input, false);
  validate(input, first, false);
  validate(input, second, false);
  require(same_summary(first.summary, second.summary));
  require(first.sink_calls == second.sink_calls);
  require(first.type_digest == second.type_digest);
  require(first.last_type == second.last_type);
  require_same_observation(replay_chunked(input, false, true), first);
  require_same_observation(replay_chunked(input, false, false), first);

  const observation stopped_first = replay(input, true);
  const observation stopped_second = replay(input, true);
  validate(input, stopped_first, true);
  validate(input, stopped_second, true);
  require(same_summary(stopped_first.summary, stopped_second.summary));
  require(stopped_first.sink_calls == stopped_second.sink_calls);
  require(stopped_first.type_digest == stopped_second.type_digest);
  require(stopped_first.last_type == stopped_second.last_type);
  require_same_observation(replay_chunked(input, true, true), stopped_first);
  require_same_observation(replay_chunked(input, true, false), stopped_first);
}

} // namespace

int feedforge_fuzz_replay_input(const std::uint8_t* const data, const std::size_t size) noexcept {
  const auto raw = std::span<const std::byte>{reinterpret_cast<const std::byte*>(data), size};
  exercise_replay(raw);

  std::array<std::byte, 4096U> decoded_storage{};
  std::span<const std::byte> decoded;
  if (feedforge::fuzz::decode_hex_seed(std::span<const std::uint8_t>{data, size}, decoded_storage,
                                       decoded)) {
    exercise_replay(decoded);
  }
  return 0;
}

#if defined(FEEDFORGE_FUZZ_STANDALONE)
int main() { return feedforge::fuzz::run_standalone_smoke(feedforge_fuzz_replay_input); }
#else
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size) noexcept {
  return feedforge_fuzz_replay_input(data, size);
}
#endif

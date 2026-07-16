#include "test_support.hpp"

#include <feedforge/generated/hardening_unknown_skip.hpp>

#include <array>
#include <cstddef>

namespace {

namespace generated = feedforge::generated::test_unknown_skip;

struct noop_sink {
  std::size_t calls{};

  template <class Event> feedforge::flow operator()(const Event&) noexcept {
    ++calls;
    return feedforge::flow::continue_;
  }
};

static_assert(generated::sink_for_all_selected_events<noop_sink>);

void check_unknown_decode_status() {
  constexpr std::array payload{std::byte{'?'}};
  noop_sink sink;
  const auto first = generated::decoder{}.decode_one(payload, sink);
  const auto second = generated::decoder{}.decode_one(payload, sink);

  FEEDFORGE_CHECK(first.status == feedforge::decode_status::unknown_skipped);
  FEEDFORGE_CHECK(first.message_type == std::byte{'?'});
  FEEDFORGE_CHECK(first.expected_size == 0U);
  FEEDFORGE_CHECK(first.actual_size == payload.size());
  FEEDFORGE_CHECK(!first.is_error() && !first.is_terminal());
  FEEDFORGE_CHECK(first.status == second.status);
  FEEDFORGE_CHECK(sink.calls == 0U);
}

void check_unknown_replay_continues() {
  constexpr std::array input = [] {
    std::array<std::byte, 16U> result{};
    result[0U] = std::byte{0U};
    result[1U] = std::byte{1U};
    result[2U] = std::byte{'?'};
    result[3U] = std::byte{0U};
    result[4U] = std::byte{9U};
    result[5U] = std::byte{'A'};
    return result;
  }();

  noop_sink sink;
  const auto summary = generated::replay_binary_file(input, sink);
  FEEDFORGE_CHECK(summary.status == feedforge::replay_status::complete);
  FEEDFORGE_CHECK(summary.frames_seen == 2U);
  FEEDFORGE_CHECK(summary.events_emitted == 1U);
  FEEDFORGE_CHECK(summary.known_messages_skipped == 0U);
  FEEDFORGE_CHECK(summary.unknown_messages_skipped == 1U);
  FEEDFORGE_CHECK(summary.bytes_consumed == input.size());
  FEEDFORGE_CHECK(summary.error_offset == 0U);
  FEEDFORGE_CHECK(summary.framing_error == feedforge::framing_errc::none);
  FEEDFORGE_CHECK(sink.calls == 1U);
}

} // namespace

int main() {
  check_unknown_decode_status();
  check_unknown_replay_continues();
}

#include <feedforge/generated/nasdaq/itch50_all.hpp>

#include <array>
#include <cstddef>

namespace {

namespace all_messages = feedforge::generated::nasdaq::itch50_all;

struct noop_sink {
  std::size_t calls{};

  template <class Event> feedforge::flow operator()(const Event&) noexcept {
    ++calls;
    return feedforge::flow::continue_;
  }
};

static_assert(all_messages::sink_for_all_selected_events<noop_sink>);

constexpr auto binary_file = [] {
  std::array<std::byte, 40U> result{};
  result[0U] = std::byte{0U};
  result[1U] = std::byte{36U};
  result[2U] = std::byte{'A'};
  return result;
}();

int exercise_hot_path() noexcept FEEDFORGE_NONBLOCKING {
  noop_sink sink;
  const auto payload = std::span<const std::byte>{binary_file}.subspan(2U, 36U);
  const auto decoded = all_messages::decoder{}.decode_one(payload, sink);
  const auto replayed = all_messages::replay_binary_file(binary_file, sink);
  return decoded.status == feedforge::decode_status::emitted &&
                 replayed.status == feedforge::replay_status::complete &&
                 replayed.events_emitted == 1U && sink.calls == 2U
             ? 0
             : 1;
}

} // namespace

int main() { return exercise_hot_path(); }

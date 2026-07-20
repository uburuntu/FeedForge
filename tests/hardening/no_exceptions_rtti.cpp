#include <feedforge/generated/nasdaq/itch50_all.hpp>

#include <array>
#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#error "The no-exceptions/RTTI gate must compile with exceptions disabled"
#endif

#if defined(__GXX_RTTI) || defined(_CPPRTTI)
#error "The no-exceptions/RTTI gate must compile with RTTI disabled"
#endif

namespace {

namespace all_messages = feedforge::generated::nasdaq::itch50_all;

struct noop_sink {
  template <class Event> feedforge::flow operator()(const Event&) noexcept {
    return feedforge::flow::continue_;
  }
};

static_assert(all_messages::sink_for_all_selected_events<noop_sink>);
static_assert(std::is_nothrow_constructible_v<all_messages::chunked_replayer<noop_sink>,
                                              std::span<std::byte>, noop_sink&>);
static_assert(noexcept(std::declval<all_messages::chunked_replayer<noop_sink>&>().push({})));
static_assert(noexcept(std::declval<all_messages::chunked_replayer<noop_sink>&>().finish()));

constexpr auto payload = [] {
  std::array<std::byte, 36U> result{};
  result[0U] = std::byte{'A'};
  return result;
}();

constexpr auto binary_file = [] {
  std::array<std::byte, 40U> result{};
  result[0U] = std::byte{0U};
  result[1U] = std::byte{36U};
  for (std::size_t index = 0U; index < payload.size(); ++index) {
    result[index + 2U] = payload[index];
  }
  return result;
}();

} // namespace

int main() {
  noop_sink sink;
  const auto decoded = all_messages::decoder{}.decode_one(payload, sink);
  const auto replayed = all_messages::replay_binary_file(binary_file, sink);
  std::array<std::byte, payload.size()> scratch{};
  all_messages::chunked_replayer<noop_sink> chunked{scratch, sink};
  static_cast<void>(chunked.push(std::span<const std::byte>{binary_file}.first(1U)));
  static_cast<void>(chunked.push(std::span<const std::byte>{binary_file}.subspan(1U)));
  const auto chunked_replayed = chunked.finish();
  return decoded.status == feedforge::decode_status::emitted &&
                 replayed.status == feedforge::replay_status::complete &&
                 replayed.frames_seen == 1U && replayed.events_emitted == 1U &&
                 chunked_replayed.status == feedforge::replay_status::complete &&
                 chunked_replayed.frames_seen == 1U && chunked_replayed.events_emitted == 1U
             ? 0
             : 1;
}

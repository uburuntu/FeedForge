#include <feedforge/generated/nasdaq/itch50_all.hpp>

#include <array>
#include <cstddef>

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
  return decoded.status == feedforge::decode_status::emitted &&
                 replayed.status == feedforge::replay_status::complete &&
                 replayed.frames_seen == 1U && replayed.events_emitted == 1U
             ? 0
             : 1;
}

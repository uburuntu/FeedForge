#include <feedforge/generated/consumer_custom.hpp>

#include <array>
#include <cstddef>

namespace events = example::consumer_custom;

struct sink {
  feedforge::flow operator()(const events::system_event& event) noexcept {
    saw_open = event.event_code.raw[0U] == 'O';
    return feedforge::flow::continue_;
  }

  bool saw_open{};
};

static_assert(events::sink_for_all_selected_events<sink>);

int main() {
  constexpr std::array input{
      std::byte{0x00}, std::byte{0x0c}, std::byte{0x53}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x03}, std::byte{0x04}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x05}, std::byte{0x06}, std::byte{0x07},
      std::byte{0x08}, std::byte{0x4f}, std::byte{0x00}, std::byte{0x00},
  };

  sink destination;
  const feedforge::replay_summary result =
      events::replay_binary_file(input, destination);
  return result.status == feedforge::replay_status::complete &&
                 result.events_emitted == 1U && destination.saw_open
             ? 0
             : 1;
}

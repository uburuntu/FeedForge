#include <feedforge/generated/nasdaq/itch50_order_events.hpp>

#include <array>
#include <cstddef>

namespace events =
    feedforge::generated::nasdaq::itch50_order_events;

struct sink {
  feedforge::flow operator()(const events::add_order& event) noexcept {
    saw_expected_order = event.order_reference_number.value == 0x11U;
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const events::add_order_mpid&) noexcept {
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const events::order_executed&) noexcept {
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(
      const events::order_executed_with_price&) noexcept {
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const events::order_cancel&) noexcept {
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const events::order_delete&) noexcept {
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const events::order_replace&) noexcept {
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const events::trade&) noexcept {
    return feedforge::flow::continue_;
  }

  bool saw_expected_order{};
};

static_assert(events::sink_for_all_selected_events<sink>);

int main() {
  constexpr std::array input{
      std::byte{0x00}, std::byte{0x24},
      std::byte{0x41}, std::byte{0x01}, std::byte{0x02}, std::byte{0x03},
      std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x05},
      std::byte{0x06}, std::byte{0x07}, std::byte{0x08}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x11}, std::byte{0x42},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x22},
      std::byte{0x54}, std::byte{0x45}, std::byte{0x53}, std::byte{0x54},
      std::byte{0x20}, std::byte{0x20}, std::byte{0x20}, std::byte{0x20},
      std::byte{0x00}, std::byte{0x01}, std::byte{0xe2}, std::byte{0x40},
      std::byte{0x00}, std::byte{0x00},
  };

  sink destination;
  const feedforge::replay_summary result =
      events::replay_binary_file(input, destination);
  return result.status == feedforge::replay_status::complete &&
                 result.events_emitted == 1U &&
                 destination.saw_expected_order
             ? 0
             : 1;
}

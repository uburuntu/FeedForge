#include <feedforge/generated/nasdaq/itch50_all.hpp>
#include <feedforge/generated/nasdaq/itch50_order_events.hpp>
#include <feedforge/version.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

namespace all = feedforge::generated::nasdaq::itch50_all;
namespace order_events =
    feedforge::generated::nasdaq::itch50_order_events;

struct no_op_sink {
  std::uint32_t calls{};

  template <class Event>
  feedforge::flow operator()(const Event&) noexcept {
    ++calls;
    return feedforge::flow::continue_;
  }
};

static_assert(all::sink_for_all_selected_events<no_op_sink>);
static_assert(order_events::sink_for_all_selected_events<no_op_sink>);

}  // namespace

int main() {
  static_assert(feedforge::runtime_api_epoch == 1);
  static_assert(feedforge::runtime_api_revision == 0);
  static_assert(all::pipeline_metadata::known_messages.size() == 23U);
  static_assert(order_events::pipeline_metadata::known_messages.size() == 23U);

  constexpr std::array payload{
      std::byte{0x41}, std::byte{0x01}, std::byte{0x02}, std::byte{0x03},
      std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x05},
      std::byte{0x06}, std::byte{0x07}, std::byte{0x08}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x11}, std::byte{0x42},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x22},
      std::byte{0x54}, std::byte{0x45}, std::byte{0x53}, std::byte{0x54},
      std::byte{0x20}, std::byte{0x20}, std::byte{0x20}, std::byte{0x20},
      std::byte{0x00}, std::byte{0x01}, std::byte{0xe2}, std::byte{0x40},
  };

  no_op_sink sink;
  const auto all_outcome = all::decoder{}.decode_one(payload, sink);
  const auto order_outcome =
      order_events::decoder{}.decode_one(payload, sink);
  return all_outcome.status == feedforge::decode_status::emitted &&
                 order_outcome.status == feedforge::decode_status::emitted &&
                 sink.calls == 2U
             ? 0
             : 1;
}

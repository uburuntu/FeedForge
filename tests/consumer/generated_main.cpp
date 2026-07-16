#if defined(FEEDFORGE_TEST_LEGACY_GENERATED_INCLUDE)
#include <consumer_custom.hpp>
#else
#include <feedforge/generated/consumer_custom.hpp>
#endif

#include <array>
#include <cstddef>

namespace generated = feedforge::generated::consumer_custom;

namespace {

struct checking_sink {
  bool called{};

  feedforge::flow operator()(const generated::system_event& event) noexcept {
    called = event.stock_locate.value == 0U &&
             event.timestamp.value == 84281096U &&
             event.event_code.raw[0U] == 'O';
    return feedforge::flow::continue_;
  }
};

static_assert(generated::sink_for_all_selected_events<checking_sink>);

}  // namespace

int main() {
  constexpr std::array payload{
      std::byte{0x53}, std::byte{0x00}, std::byte{0x00}, std::byte{0x03},
      std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x05},
      std::byte{0x06}, std::byte{0x07}, std::byte{0x08}, std::byte{0x4f},
  };
  checking_sink sink;
  const auto outcome = generated::decoder{}.decode_one(payload, sink);
  return outcome.status == feedforge::decode_status::emitted && sink.called
             ? 0
             : 1;
}

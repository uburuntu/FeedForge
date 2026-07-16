#include <feedforge/generated/nasdaq/itch50_all.hpp>
#include <feedforge/generated/nasdaq/itch50_order_events.hpp>

#include <cstdint>
#include <cstdio>
#include <string_view>
#include <type_traits>

namespace all_messages = feedforge::generated::nasdaq::itch50_all;
namespace order_events = feedforge::generated::nasdaq::itch50_order_events;

struct compile_sink {
  template <class Event> feedforge::flow operator()(const Event&) noexcept {
    return feedforge::flow::continue_;
  }
};

template <class Event>
concept has_tracking_number = requires(Event event) { event.tracking_number; };

template <class Event>
concept has_attribution = requires(Event event) { event.attribution; };

static_assert(all_messages::sink_for_all_selected_events<compile_sink>);
static_assert(order_events::sink_for_all_selected_events<compile_sink>);
static_assert(std::is_trivially_copyable_v<all_messages::system_event>);
static_assert(std::is_trivially_copyable_v<order_events::add_order>);
static_assert(has_tracking_number<all_messages::add_order>);
static_assert(!has_tracking_number<order_events::add_order>);
static_assert(has_attribution<order_events::add_order_mpid>);
static_assert(!has_attribution<order_events::add_order>);
static_assert(all_messages::pipeline_metadata::required_runtime_api_version == std::uint32_t{1U});
static_assert(order_events::pipeline_metadata::required_runtime_api_version == std::uint32_t{1U});
static_assert(all_messages::pipeline_metadata::generator_version == std::string_view{"0.1.0"});
static_assert(order_events::pipeline_metadata::generator_version == std::string_view{"0.1.0"});

template <class Event> void print_layout() noexcept {
  std::printf("%zu:%zu;", sizeof(Event), alignof(Event));
}

void print_layout_fingerprint() noexcept {
  print_layout<all_messages::add_order>();
  print_layout<all_messages::broken_trade>();
  print_layout<all_messages::order_executed_with_price>();
  print_layout<all_messages::order_delete>();
  print_layout<all_messages::order_executed>();
  print_layout<all_messages::add_order_mpid>();
  print_layout<all_messages::stock_trading_action>();
  print_layout<all_messages::net_order_imbalance_indicator>();
  print_layout<all_messages::luld_auction_collar>();
  print_layout<all_messages::ipo_quoting_period_update>();
  print_layout<all_messages::market_participant_position>();
  print_layout<all_messages::retail_price_improvement_indicator>();
  print_layout<all_messages::direct_listing_with_capital_raise>();
  print_layout<all_messages::trade>();
  print_layout<all_messages::cross_trade>();
  print_layout<all_messages::stock_directory>();
  print_layout<all_messages::system_event>();
  print_layout<all_messages::order_replace>();
  print_layout<all_messages::mwcb_decline_level>();
  print_layout<all_messages::mwcb_status>();
  print_layout<all_messages::order_cancel>();
  print_layout<all_messages::reg_sho_restriction>();
  print_layout<all_messages::operational_halt>();
  print_layout<order_events::add_order>();
  print_layout<order_events::add_order_mpid>();
  print_layout<order_events::order_executed>();
  print_layout<order_events::order_executed_with_price>();
  print_layout<order_events::order_cancel>();
  print_layout<order_events::order_delete>();
  print_layout<order_events::order_replace>();
  print_layout<order_events::trade>();
  std::puts("");
}

int main() {
  compile_sink sink;
  const std::span<const std::byte> empty;
  const auto all_decode = all_messages::decoder{}.decode_one(empty, sink);
  const auto order_decode = order_events::decoder{}.decode_one(empty, sink);
  const auto all_replay = all_messages::replay_binary_file(empty, sink);
  const auto order_replay = order_events::replay_binary_file(empty, sink);
  print_layout_fingerprint();
  return all_messages::pipeline_metadata::known_messages.size() == 23U &&
                 order_events::pipeline_metadata::known_messages.size() == 23U &&
                 all_decode.status == feedforge::decode_status::empty_payload &&
                 order_decode.status == feedforge::decode_status::empty_payload &&
                 all_replay.status == feedforge::replay_status::incomplete &&
                 order_replay.status == feedforge::replay_status::incomplete
             ? 0
             : 1;
}

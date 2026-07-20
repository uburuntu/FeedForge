#include <feedforge/generated/nasdaq/itch50_order_events.hpp>

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <span>
#include <string_view>

namespace {

namespace order_events = feedforge::generated::nasdaq::itch50_order_events;

// A format-valid synthetic BinaryFILE session: add, system event, execute,
// replace, and the required zero-length end marker. It contains no captured
// or licensed exchange data.
constexpr char synthetic_session[] =
    // Add Order: buy 250 FFORGE at 185.2500, order 1001.
    "\x00\x24\x41\x00\x07\x00\x01\x00\x00\x00"
    "\x0f\x42\x40\x00\x00\x00\x00\x00\x00\x03"
    "\xe9\x42\x00\x00\x00\xfa\x46\x46\x4f\x52"
    "\x47\x45\x20\x20\x00\x1c\x44\x54"

    // System Event: structurally valid but intentionally not projected.
    "\x00\x0c\x53\x00\x00\x00\x02\x00\x00\x00"
    "\x0f\x43\x3a\x4f"

    // Order Executed: execute 100 shares of order 1001, match 9001.
    "\x00\x1f\x45\x00\x07\x00\x03\x00\x00\x00"
    "\x0f\x44\x34\x00\x00\x00\x00\x00\x00\x03"
    "\xe9\x00\x00\x00\x64\x00\x00\x00\x00\x00"
    "\x00\x23\x29"

    // Order Replace: order 1001 becomes 1002, 150 shares at 185.3000.
    "\x00\x23\x55\x00\x07\x00\x04\x00\x00\x00"
    "\x0f\x46\x28\x00\x00\x00\x00\x00\x00\x03"
    "\xe9\x00\x00\x00\x00\x00\x00\x03\xea\x00"
    "\x00\x00\x96\x00\x1c\x46\x48"

    "\x00\x00";

constexpr std::size_t synthetic_session_size = sizeof(synthetic_session) - 1U;
static_assert(synthetic_session_size == 124U);

void print_price(const feedforge::decimal<std::uint32_t, 4> price) noexcept {
  constexpr std::uint32_t scale = 10'000U;
  std::cout << price.raw / scale << '.' << std::setfill('0') << std::setw(4) << price.raw % scale
            << std::setfill(' ');
}

[[nodiscard]] constexpr std::string_view name(const feedforge::replay_status status) noexcept {
  using enum feedforge::replay_status;
  switch (status) {
  case complete:
    return "complete";
  case incomplete:
    return "incomplete";
  case stopped:
    return "stopped";
  case framing_error:
    return "framing_error";
  case decode_error:
    return "decode_error";
  }
  return "unknown";
}

struct showcase_sink {
  feedforge::flow operator()(const order_events::add_order& event) noexcept {
    valid = valid && events_seen == 0U && event.order_reference_number.value == 1001U &&
            event.buy_sell_indicator.trimmed() == "B" && event.shares.value == 250U &&
            event.stock.trimmed() == "FFORGE" && event.price.raw == 1'852'500U &&
            event.timestamp.value == 1'000'000U;
    ++events_seen;

    std::cout << "event=" << event.event_name << " order=" << event.order_reference_number.value
              << " side=buy shares=" << event.shares.value << " stock=" << event.stock.trimmed()
              << " price=";
    print_price(event.price);
    std::cout << " timestamp_ns=" << event.timestamp.value << '\n';
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const order_events::order_executed& event) noexcept {
    valid = valid && events_seen == 1U && event.order_reference_number.value == 1001U &&
            event.executed_shares.value == 100U && event.match_number.value == 9001U &&
            event.timestamp.value == 1'000'500U;
    ++events_seen;

    std::cout << "event=" << event.event_name << " order=" << event.order_reference_number.value
              << " executed_shares=" << event.executed_shares.value
              << " match=" << event.match_number.value << " timestamp_ns=" << event.timestamp.value
              << '\n';
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const order_events::order_replace& event) noexcept {
    valid = valid && events_seen == 2U && event.original_order_reference_number.value == 1001U &&
            event.new_order_reference_number.value == 1002U && event.shares.value == 150U &&
            event.price.raw == 1'853'000U && event.timestamp.value == 1'001'000U;
    ++events_seen;

    std::cout << "event=" << event.event_name
              << " old_order=" << event.original_order_reference_number.value
              << " new_order=" << event.new_order_reference_number.value
              << " shares=" << event.shares.value << " price=";
    print_price(event.price);
    std::cout << " timestamp_ns=" << event.timestamp.value << '\n';
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const order_events::add_order_mpid&) noexcept { return unexpected(); }

  feedforge::flow operator()(const order_events::order_executed_with_price&) noexcept {
    return unexpected();
  }

  feedforge::flow operator()(const order_events::order_cancel&) noexcept { return unexpected(); }

  feedforge::flow operator()(const order_events::order_delete&) noexcept { return unexpected(); }

  feedforge::flow operator()(const order_events::trade&) noexcept { return unexpected(); }

  std::uint64_t events_seen{};
  bool valid{true};

private:
  feedforge::flow unexpected() noexcept {
    valid = false;
    ++events_seen;
    return feedforge::flow::continue_;
  }
};

static_assert(order_events::sink_for_all_selected_events<showcase_sink>);

} // namespace

int main() {
  std::cout << "FeedForge synthetic order-event showcase\n"
            << "source=embedded format-valid synthetic bytes; "
               "no captured exchange data\n";

  showcase_sink sink;
  const auto input = std::as_bytes(std::span{synthetic_session, synthetic_session_size});
  const feedforge::replay_summary summary = order_events::replay_binary_file(input, sink);

  std::cout << "summary status=" << name(summary.status) << " frames=" << summary.frames_seen
            << " emitted=" << summary.events_emitted
            << " known_skipped=" << summary.known_messages_skipped
            << " bytes=" << summary.bytes_consumed << '\n';

  const bool expected =
      summary.status == feedforge::replay_status::complete && summary.frames_seen == 4U &&
      summary.events_emitted == 3U && summary.known_messages_skipped == 1U &&
      summary.unknown_messages_skipped == 0U && summary.bytes_consumed == synthetic_session_size &&
      sink.events_seen == 3U && sink.valid;
  return expected ? 0 : 1;
}

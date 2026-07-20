#include <feedforge/generated/nasdaq/itch50_order_events.hpp>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace {

namespace order_events =
    feedforge::generated::nasdaq::itch50_order_events;

[[nodiscard]] constexpr std::string_view name(
    const feedforge::replay_status status) noexcept {
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

[[nodiscard]] constexpr std::string_view name(
    const feedforge::framing_errc error) noexcept {
  using enum feedforge::framing_errc;
  switch (error) {
    case none:
      return "none";
    case truncated_length_prefix:
      return "truncated_length_prefix";
    case truncated_payload:
      return "truncated_payload";
    case trailing_data_after_end_marker:
      return "trailing_data_after_end_marker";
    case insufficient_scratch:
      return "insufficient_scratch";
    }
  return "unknown";
}

[[nodiscard]] constexpr std::string_view name(
    const feedforge::decode_status status) noexcept {
  using enum feedforge::decode_status;
  switch (status) {
    case emitted:
      return "emitted";
    case known_unselected_skipped:
      return "known_unselected_skipped";
    case unknown_skipped:
      return "unknown_skipped";
    case stopped:
      return "stopped";
    case empty_payload:
      return "empty_payload";
    case unknown_message_type:
      return "unknown_message_type";
    case invalid_message_size:
      return "invalid_message_size";
  }
  return "unknown";
}

struct counting_sink {
  std::uint64_t events_seen{};

  feedforge::flow operator()(const order_events::add_order&) noexcept {
    ++events_seen;
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const order_events::add_order_mpid&) noexcept {
    ++events_seen;
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(
      const order_events::order_executed&) noexcept {
    ++events_seen;
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(
      const order_events::order_executed_with_price&) noexcept {
    ++events_seen;
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const order_events::order_cancel&) noexcept {
    ++events_seen;
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const order_events::order_delete&) noexcept {
    ++events_seen;
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const order_events::order_replace&) noexcept {
    ++events_seen;
    return feedforge::flow::continue_;
  }

  feedforge::flow operator()(const order_events::trade&) noexcept {
    ++events_seen;
    return feedforge::flow::continue_;
  }
};

static_assert(order_events::sink_for_all_selected_events<counting_sink>);

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: feedforge-replay <binary-file>\n";
    return 2;
  }

  // This local CLI intentionally opens the exact input path selected by its
  // caller; it has no privileged or fixed-directory path boundary.
  std::ifstream input{argv[1], std::ios::binary | std::ios::ate};
  if (!input) {
    std::cerr << "unable to open " << argv[1] << '\n';
    return 3;
  }

  const std::streamoff end = input.tellg();
  if (end < 0 ||
      static_cast<std::uintmax_t>(end) >
          static_cast<std::uintmax_t>(
              std::numeric_limits<std::size_t>::max()) ||
      static_cast<std::uintmax_t>(end) >
          static_cast<std::uintmax_t>(
              std::numeric_limits<std::streamsize>::max())) {
    std::cerr << "unable to determine a supported input size\n";
    return 3;
  }

  std::vector<std::byte> storage(static_cast<std::size_t>(end));
  input.seekg(0, std::ios::beg);
  if (!storage.empty()) {
    input.read(reinterpret_cast<char*>(storage.data()),
               static_cast<std::streamsize>(storage.size()));
  }
  if (!input) {
    std::cerr << "unable to read " << argv[1] << '\n';
    return 3;
  }

  counting_sink sink;
  const feedforge::replay_summary summary =
      order_events::replay_binary_file(
          std::span<const std::byte>{storage.data(), storage.size()}, sink);

  std::cout << "status=" << name(summary.status)
            << " frames_seen=" << summary.frames_seen
            << " events_emitted=" << summary.events_emitted
            << " known_messages_skipped="
            << summary.known_messages_skipped
            << " unknown_messages_skipped="
            << summary.unknown_messages_skipped
            << " bytes_consumed=" << summary.bytes_consumed << '\n';

  if (summary.status == feedforge::replay_status::framing_error) {
    std::cerr << "framing_error=" << name(summary.framing_error)
              << " error_offset=" << summary.error_offset << '\n';
    return 1;
  }
  if (summary.status == feedforge::replay_status::decode_error) {
    const auto type =
        std::to_integer<unsigned int>(summary.decode_error.message_type);
    std::cerr << "decode_error=" << name(summary.decode_error.status)
              << " message_type=0x" << std::hex << std::setw(2)
              << std::setfill('0') << type << std::dec
              << " expected_size=" << summary.decode_error.expected_size
              << " actual_size=" << summary.decode_error.actual_size
              << " error_offset=" << summary.error_offset << '\n';
    return 1;
  }
  return sink.events_seen == summary.events_emitted ? 0 : 1;
}

#ifndef FEEDFORGE_TESTS_REFERENCE_ITCH50_ORACLE_HPP
#define FEEDFORGE_TESTS_REFERENCE_ITCH50_ORACLE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

// This namespace deliberately has no dependency on FeedForge. The message
// facts below are a second transcription of the public ITCH 5.0 layout, used
// only as a test oracle.
namespace feedforge_reference::itch50 {

[[nodiscard]] constexpr std::byte octet(const char value) noexcept {
  return std::byte{static_cast<unsigned char>(value)};
}

struct message_layout {
  std::byte discriminator{};
  std::uint16_t size{};
  std::string_view name;
};

inline constexpr std::array message_layouts{
    message_layout{octet('S'), 12U, "system_event"},
    message_layout{octet('R'), 39U, "stock_directory"},
    message_layout{octet('H'), 25U, "stock_trading_action"},
    message_layout{octet('Y'), 20U, "reg_sho_restriction"},
    message_layout{octet('L'), 26U, "market_participant_position"},
    message_layout{octet('V'), 35U, "mwcb_decline_level"},
    message_layout{octet('W'), 12U, "mwcb_status"},
    message_layout{octet('K'), 28U, "ipo_quoting_period_update"},
    message_layout{octet('J'), 35U, "luld_auction_collar"},
    message_layout{octet('h'), 21U, "operational_halt"},
    message_layout{octet('A'), 36U, "add_order"},
    message_layout{octet('F'), 40U, "add_order_mpid"},
    message_layout{octet('E'), 31U, "order_executed"},
    message_layout{octet('C'), 36U, "order_executed_with_price"},
    message_layout{octet('X'), 23U, "order_cancel"},
    message_layout{octet('D'), 19U, "order_delete"},
    message_layout{octet('U'), 35U, "order_replace"},
    message_layout{octet('P'), 44U, "trade"},
    message_layout{octet('Q'), 40U, "cross_trade"},
    message_layout{octet('B'), 19U, "broken_trade"},
    message_layout{octet('I'), 50U, "net_order_imbalance_indicator"},
    message_layout{octet('N'), 20U, "retail_price_improvement_indicator"},
    message_layout{octet('O'), 48U, "direct_listing_with_capital_raise"},
};

inline constexpr std::size_t maximum_message_size = 50U;

[[nodiscard]] constexpr const message_layout* find_layout(const std::byte discriminator) noexcept {
  for (const message_layout& layout : message_layouts) {
    if (layout.discriminator == discriminator) {
      return &layout;
    }
  }
  return nullptr;
}

enum class decode_status : std::uint8_t {
  emitted,
  empty_payload,
  unknown_message_type,
  invalid_message_size,
};

struct decode_outcome {
  decode_status status{};
  std::byte message_type{};
  std::uint16_t expected_size{};
  std::size_t actual_size{};
};

[[nodiscard]] constexpr decode_outcome classify(const std::span<const std::byte> payload) noexcept {
  if (payload.empty()) {
    return {decode_status::empty_payload, std::byte{0U}, 0U, 0U};
  }

  const std::byte message_type = payload.front();
  const message_layout* const layout = find_layout(message_type);
  if (layout == nullptr) {
    return {decode_status::unknown_message_type, message_type, 0U, payload.size()};
  }
  if (payload.size() != layout->size) {
    return {decode_status::invalid_message_size, message_type, layout->size, payload.size()};
  }
  return {decode_status::emitted, message_type, layout->size, payload.size()};
}

struct unsigned_read {
  std::uint64_t value{};
  bool valid{};
};

// Kept intentionally separate from feedforge::wire. This loop is the complete
// reference implementation for unsigned big-endian fields.
[[nodiscard]] constexpr unsigned_read read_unsigned(const std::span<const std::byte> payload,
                                                    const std::size_t offset,
                                                    const std::size_t width) noexcept {
  if (width == 0U || width > sizeof(std::uint64_t) || offset > payload.size() ||
      width > payload.size() - offset) {
    return {};
  }

  std::uint64_t result = 0U;
  for (std::size_t index = 0U; index < width; ++index) {
    result = (result << 8U) | std::to_integer<std::uint64_t>(payload[offset + index]);
  }
  return {result, true};
}

[[nodiscard]] constexpr bool bytes_equal(const std::span<const std::byte> payload,
                                         const std::size_t offset,
                                         const std::span<const char> actual) noexcept {
  if (offset > payload.size() || actual.size() > payload.size() - offset) {
    return false;
  }
  for (std::size_t index = 0U; index < actual.size(); ++index) {
    const auto actual_byte = std::byte{static_cast<unsigned char>(actual[index])};
    if (actual_byte != payload[offset + index]) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] consteval bool layouts_are_well_formed() {
  if (message_layouts.size() != 23U) {
    return false;
  }
  for (std::size_t left = 0U; left < message_layouts.size(); ++left) {
    const message_layout& layout = message_layouts[left];
    if (layout.size == 0U || layout.size > maximum_message_size || layout.name.empty()) {
      return false;
    }
    for (std::size_t right = left + 1U; right < message_layouts.size(); ++right) {
      if (layout.discriminator == message_layouts[right].discriminator) {
        return false;
      }
    }
  }
  return true;
}

static_assert(layouts_are_well_formed());

} // namespace feedforge_reference::itch50

#endif // FEEDFORGE_TESTS_REFERENCE_ITCH50_ORACLE_HPP

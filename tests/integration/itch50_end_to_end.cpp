#include <feedforge/generated/nasdaq/itch50_all.hpp>
#include <feedforge/generated/nasdaq/itch50_order_events.hpp>

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

namespace all_messages = feedforge::generated::nasdaq::itch50_all;
namespace order_events =
    feedforge::generated::nasdaq::itch50_order_events;

struct fixture {
  char message_type{};
  std::string message_name;
  std::vector<std::byte> bytes;
  std::size_t raw_size{};
  std::map<std::string, std::string, std::less<>> expected_fields;
  std::string order_result;
  std::string order_event;
};

int failures = 0;

void check(const bool condition, const std::string_view description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    ++failures;
  }
}

void check_field(const bool condition, const fixture& expected,
                 const std::string_view field) noexcept {
  if (!condition) {
    std::cerr << "FAIL: " << expected.message_name << '.' << field << '\n';
    ++failures;
  }
}

[[nodiscard]] constexpr std::string_view trim(
    std::string_view value) noexcept {
  while (!value.empty() &&
         (value.front() == ' ' || value.front() == '\t' ||
          value.front() == '\r' || value.front() == '\n')) {
    value.remove_prefix(1U);
  }
  while (!value.empty() &&
         (value.back() == ' ' || value.back() == '\t' ||
          value.back() == '\r' || value.back() == '\n')) {
    value.remove_suffix(1U);
  }
  return value;
}

[[nodiscard]] std::string parse_manifest_value(
    const std::string_view line) {
  const std::size_t equals = line.find('=');
  if (equals == std::string_view::npos) {
    return {};
  }
  std::string_view value = trim(line.substr(equals + 1U));
  if (value.size() >= 2U && value.front() == '"' && value.back() == '"') {
    value.remove_prefix(1U);
    value.remove_suffix(1U);
  }
  return std::string{value};
}

[[nodiscard]] std::uint64_t parse_unsigned(
    const std::string_view text, const fixture& expected,
    const std::string_view field) noexcept {
  std::uint64_t value{};
  const auto parsed =
      std::from_chars(text.data(), text.data() + text.size(), value, 10);
  check_field(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size(),
              expected, field);
  return value;
}

[[nodiscard]] std::vector<std::byte> parse_hex(
    const std::string_view text, const std::string_view file_name) {
  std::vector<std::byte> result;
  std::size_t position = 0U;
  while (position < text.size()) {
    while (position < text.size() && text[position] == ' ') {
      ++position;
    }
    if (position == text.size()) {
      break;
    }
    const std::size_t end = text.find(' ', position);
    const std::size_t token_end =
        end == std::string_view::npos ? text.size() : end;
    unsigned int value{};
    const auto parsed =
        std::from_chars(text.data() + position, text.data() + token_end, value,
                        16);
    check(parsed.ec == std::errc{} &&
              parsed.ptr == text.data() + token_end && value <= 0xffU,
          file_name);
    result.push_back(std::byte{static_cast<unsigned char>(value)});
    position = token_end;
  }
  return result;
}

[[nodiscard]] fixture load_fixture(const std::string_view file_name) {
  const std::string path =
      std::string{FEEDFORGE_ITCH50_FIXTURE_DIR} + '/' +
      std::string{file_name};
  std::ifstream input{path};
  check(static_cast<bool>(input), file_name);

  enum class section {
    top,
    expected_fields,
    expected_order_events,
    other,
  };
  section current = section::top;
  fixture loaded;
  std::string line;
  while (std::getline(input, line)) {
    const std::string_view current_line = trim(line);
    if (current_line.empty()) {
      continue;
    }
    if (current_line.front() == '[') {
      if (current_line == "[expected_fields]") {
        current = section::expected_fields;
      } else if (current_line == "[expected_order_events]") {
        current = section::expected_order_events;
      } else {
        current = section::other;
      }
      continue;
    }

    const std::size_t equals = current_line.find('=');
    if (equals == std::string_view::npos) {
      continue;
    }
    const std::string_view key = trim(current_line.substr(0U, equals));
    const std::string value = parse_manifest_value(current_line);
    if (current == section::top) {
      if (key == "message_type") {
        check(value.size() == 1U, file_name);
        if (value.size() == 1U) {
          loaded.message_type = value[0U];
        }
      } else if (key == "message_name") {
        loaded.message_name = value;
      } else if (key == "raw_hex") {
        loaded.bytes = parse_hex(value, file_name);
      } else if (key == "raw_size") {
        loaded.raw_size =
            static_cast<std::size_t>(parse_unsigned(value, loaded, key));
      }
    } else if (current == section::expected_fields) {
      loaded.expected_fields.emplace(std::string{key}, value);
    } else if (current == section::expected_order_events) {
      if (key == "result") {
        loaded.order_result = value;
      } else if (key == "event") {
        loaded.order_event = value;
      }
    }
  }

  check(loaded.bytes.size() == loaded.raw_size, file_name);
  check(!loaded.bytes.empty() &&
            loaded.bytes.front() ==
                std::byte{static_cast<unsigned char>(loaded.message_type)},
        file_name);
  check(loaded.order_result == "emit" || loaded.order_result == "skip",
        file_name);
  return loaded;
}

[[nodiscard]] std::vector<fixture> load_fixtures() {
  constexpr std::array files{
      std::string_view{"01_system_event.toml"},
      std::string_view{"02_stock_directory.toml"},
      std::string_view{"03_stock_trading_action.toml"},
      std::string_view{"04_reg_sho_restriction.toml"},
      std::string_view{"05_market_participant_position.toml"},
      std::string_view{"06_mwcb_decline_level.toml"},
      std::string_view{"07_mwcb_status.toml"},
      std::string_view{"08_ipo_quoting_period_update.toml"},
      std::string_view{"09_luld_auction_collar.toml"},
      std::string_view{"10_operational_halt.toml"},
      std::string_view{"11_add_order.toml"},
      std::string_view{"12_add_order_mpid.toml"},
      std::string_view{"13_order_executed.toml"},
      std::string_view{"14_order_executed_with_price.toml"},
      std::string_view{"15_order_cancel.toml"},
      std::string_view{"16_order_delete.toml"},
      std::string_view{"17_order_replace.toml"},
      std::string_view{"18_trade.toml"},
      std::string_view{"19_cross_trade.toml"},
      std::string_view{"20_broken_trade.toml"},
      std::string_view{"21_net_order_imbalance_indicator.toml"},
      std::string_view{"22_retail_price_improvement_indicator.toml"},
      std::string_view{"23_direct_listing_with_capital_raise.toml"},
  };
  std::vector<fixture> result;
  result.reserve(files.size());
  for (const std::string_view file : files) {
    result.push_back(load_fixture(file));
  }
  return result;
}

[[nodiscard]] const std::string& expected_value(
    const fixture& expected, const std::string_view field) noexcept {
  const auto found = expected.expected_fields.find(field);
  if (found == expected.expected_fields.end()) {
    check_field(false, expected, field);
    static const std::string empty;
    return empty;
  }
  return found->second;
}

template <class Value>
[[nodiscard]] constexpr std::uint64_t numeric_value(
    const Value& value) noexcept {
  if constexpr (requires { value.value; }) {
    return static_cast<std::uint64_t>(value.value);
  } else if constexpr (requires { value.raw; }) {
    return static_cast<std::uint64_t>(value.raw);
  } else {
    return static_cast<std::uint64_t>(value);
  }
}

template <class Value>
void expect_number(const Value& actual, const fixture& expected,
                   const std::string_view field) noexcept {
  const std::string& text = expected_value(expected, field);
  check_field(numeric_value(actual) ==
                  parse_unsigned(text, expected, field),
              expected, field);
}

template <std::size_t Size>
void expect_ascii(const feedforge::ascii<Size>& actual,
                  const fixture& expected,
                  const std::string_view field) noexcept {
  const std::string& text = expected_value(expected, field);
  bool equal = text.size() == Size;
  for (std::size_t index = 0U; equal && index < Size; ++index) {
    equal = actual.raw[index] == text[index];
  }
  check_field(equal, expected, field);
}

template <class Event>
void expect_identity(const Event&, const fixture& expected) noexcept {
  check_field(
      Event::source_discriminator ==
          std::byte{static_cast<unsigned char>(expected.message_type)},
      expected, "source_discriminator");
  check_field(Event::event_name == expected.message_name, expected,
              "event_name");
}

template <class Event>
void expect_all_common(const Event& event,
                       const fixture& expected) noexcept {
  expect_number(event.stock_locate, expected, "stock_locate");
  expect_number(event.tracking_number, expected, "tracking_number");
  expect_number(event.timestamp, expected, "timestamp");
}

void verify_all(const all_messages::system_event& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_ascii(event.event_code, expected, "event_code");
}

void verify_all(const all_messages::stock_directory& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_ascii(event.stock, expected, "stock");
  expect_ascii(event.market_category, expected, "market_category");
  expect_ascii(event.financial_status_indicator, expected,
               "financial_status_indicator");
  expect_number(event.round_lot_size, expected, "round_lot_size");
  expect_ascii(event.round_lots_only, expected, "round_lots_only");
  expect_ascii(event.issue_classification, expected,
               "issue_classification");
  expect_ascii(event.issue_sub_type, expected, "issue_sub_type");
  expect_ascii(event.authenticity, expected, "authenticity");
  expect_ascii(event.short_sale_threshold_indicator, expected,
               "short_sale_threshold_indicator");
  expect_ascii(event.ipo_flag, expected, "ipo_flag");
  expect_ascii(event.luld_reference_price_tier, expected,
               "luld_reference_price_tier");
  expect_ascii(event.etp_flag, expected, "etp_flag");
  expect_number(event.etp_leverage_factor, expected,
                "etp_leverage_factor");
  expect_ascii(event.inverse_indicator, expected, "inverse_indicator");
}

void verify_all(const all_messages::stock_trading_action& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_ascii(event.stock, expected, "stock");
  expect_ascii(event.trading_state, expected, "trading_state");
  expect_ascii(event.reason, expected, "reason");
}

void verify_all(const all_messages::reg_sho_restriction& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_ascii(event.stock, expected, "stock");
  expect_ascii(event.reg_sho_action, expected, "reg_sho_action");
}

void verify_all(const all_messages::market_participant_position& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_ascii(event.mpid, expected, "mpid");
  expect_ascii(event.stock, expected, "stock");
  expect_ascii(event.primary_market_maker, expected,
               "primary_market_maker");
  expect_ascii(event.market_maker_mode, expected, "market_maker_mode");
  expect_ascii(event.market_participant_state, expected,
               "market_participant_state");
}

void verify_all(const all_messages::mwcb_decline_level& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_number(event.level_1, expected, "level_1");
  expect_number(event.level_2, expected, "level_2");
  expect_number(event.level_3, expected, "level_3");
}

void verify_all(const all_messages::mwcb_status& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_ascii(event.breached_level, expected, "breached_level");
}

void verify_all(const all_messages::ipo_quoting_period_update& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_ascii(event.stock, expected, "stock");
  expect_number(event.ipo_quotation_release_time, expected,
                "ipo_quotation_release_time");
  expect_ascii(event.ipo_quotation_release_qualifier, expected,
               "ipo_quotation_release_qualifier");
  expect_number(event.ipo_price, expected, "ipo_price");
}

void verify_all(const all_messages::luld_auction_collar& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_ascii(event.stock, expected, "stock");
  expect_number(event.auction_collar_reference_price, expected,
                "auction_collar_reference_price");
  expect_number(event.upper_auction_collar_price, expected,
                "upper_auction_collar_price");
  expect_number(event.lower_auction_collar_price, expected,
                "lower_auction_collar_price");
  expect_number(event.auction_collar_extension, expected,
                "auction_collar_extension");
}

void verify_all(const all_messages::operational_halt& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_ascii(event.stock, expected, "stock");
  expect_ascii(event.market_code, expected, "market_code");
  expect_ascii(event.operational_halt_action, expected,
               "operational_halt_action");
}

void verify_all(const all_messages::add_order& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_number(event.order_reference_number, expected,
                "order_reference_number");
  expect_ascii(event.buy_sell_indicator, expected, "buy_sell_indicator");
  expect_number(event.shares, expected, "shares");
  expect_ascii(event.stock, expected, "stock");
  expect_number(event.price, expected, "price");
}

void verify_all(const all_messages::add_order_mpid& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_number(event.order_reference_number, expected,
                "order_reference_number");
  expect_ascii(event.buy_sell_indicator, expected, "buy_sell_indicator");
  expect_number(event.shares, expected, "shares");
  expect_ascii(event.stock, expected, "stock");
  expect_number(event.price, expected, "price");
  expect_ascii(event.attribution, expected, "attribution");
}

void verify_all(const all_messages::order_executed& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_number(event.order_reference_number, expected,
                "order_reference_number");
  expect_number(event.executed_shares, expected, "executed_shares");
  expect_number(event.match_number, expected, "match_number");
}

void verify_all(const all_messages::order_executed_with_price& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_number(event.order_reference_number, expected,
                "order_reference_number");
  expect_number(event.executed_shares, expected, "executed_shares");
  expect_number(event.match_number, expected, "match_number");
  expect_ascii(event.printable, expected, "printable");
  expect_number(event.execution_price, expected, "execution_price");
}

void verify_all(const all_messages::order_cancel& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_number(event.order_reference_number, expected,
                "order_reference_number");
  expect_number(event.cancelled_shares, expected, "cancelled_shares");
}

void verify_all(const all_messages::order_delete& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_number(event.order_reference_number, expected,
                "order_reference_number");
}

void verify_all(const all_messages::order_replace& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_number(event.original_order_reference_number, expected,
                "original_order_reference_number");
  expect_number(event.new_order_reference_number, expected,
                "new_order_reference_number");
  expect_number(event.shares, expected, "shares");
  expect_number(event.price, expected, "price");
}

void verify_all(const all_messages::trade& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_number(event.order_reference_number, expected,
                "order_reference_number");
  expect_ascii(event.buy_sell_indicator, expected, "buy_sell_indicator");
  expect_number(event.shares, expected, "shares");
  expect_ascii(event.stock, expected, "stock");
  expect_number(event.price, expected, "price");
  expect_number(event.match_number, expected, "match_number");
}

void verify_all(const all_messages::cross_trade& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_number(event.shares, expected, "shares");
  expect_ascii(event.stock, expected, "stock");
  expect_number(event.cross_price, expected, "cross_price");
  expect_number(event.match_number, expected, "match_number");
  expect_ascii(event.cross_type, expected, "cross_type");
}

void verify_all(const all_messages::broken_trade& event,
                const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_number(event.match_number, expected, "match_number");
}

void verify_all(
    const all_messages::net_order_imbalance_indicator& event,
    const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_number(event.paired_shares, expected, "paired_shares");
  expect_number(event.imbalance_shares, expected, "imbalance_shares");
  expect_ascii(event.imbalance_direction, expected, "imbalance_direction");
  expect_ascii(event.stock, expected, "stock");
  expect_number(event.far_price, expected, "far_price");
  expect_number(event.near_price, expected, "near_price");
  expect_number(event.current_reference_price, expected,
                "current_reference_price");
  expect_ascii(event.cross_type, expected, "cross_type");
  expect_ascii(event.price_variation_indicator, expected,
               "price_variation_indicator");
}

void verify_all(
    const all_messages::retail_price_improvement_indicator& event,
    const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_ascii(event.stock, expected, "stock");
  expect_ascii(event.interest_flag, expected, "interest_flag");
}

void verify_all(
    const all_messages::direct_listing_with_capital_raise& event,
    const fixture& expected) noexcept {
  expect_all_common(event, expected);
  expect_ascii(event.stock, expected, "stock");
  expect_ascii(event.open_eligibility_status, expected,
               "open_eligibility_status");
  expect_number(event.minimum_allowable_price, expected,
                "minimum_allowable_price");
  expect_number(event.maximum_allowable_price, expected,
                "maximum_allowable_price");
  expect_number(event.near_execution_price, expected,
                "near_execution_price");
  expect_number(event.near_execution_time, expected, "near_execution_time");
  expect_number(event.lower_price_range_collar, expected,
                "lower_price_range_collar");
  expect_number(event.upper_price_range_collar, expected,
                "upper_price_range_collar");
}

[[nodiscard]] const fixture* find_fixture(
    const std::vector<fixture>& fixtures, const std::byte type) noexcept {
  for (const fixture& candidate : fixtures) {
    if (type == std::byte{static_cast<unsigned char>(candidate.message_type)}) {
      return &candidate;
    }
  }
  return nullptr;
}

struct all_sink {
  const fixture* expected{};
  const std::vector<fixture>* fixtures{};
  std::size_t calls{};

  template <class Event>
  feedforge::flow operator()(const Event& event) noexcept {
    ++calls;
    const fixture* selected =
        expected != nullptr
            ? expected
            : find_fixture(*fixtures, Event::source_discriminator);
    if (selected == nullptr) {
      check(false, "all_messages sink received unknown event type");
      return feedforge::flow::continue_;
    }
    expect_identity(event, *selected);
    verify_all(event, *selected);
    return feedforge::flow::continue_;
  }
};

static_assert(all_messages::sink_for_all_selected_events<all_sink>);

template <class Event>
void expect_order_common(const Event& event,
                         const fixture& expected) noexcept {
  expect_number(event.stock_locate, expected, "stock_locate");
  expect_number(event.timestamp, expected, "timestamp");
}

void verify_order(const order_events::add_order& event,
                  const fixture& expected) noexcept {
  expect_order_common(event, expected);
  expect_number(event.order_reference_number, expected,
                "order_reference_number");
  expect_ascii(event.buy_sell_indicator, expected, "buy_sell_indicator");
  expect_number(event.shares, expected, "shares");
  expect_ascii(event.stock, expected, "stock");
  expect_number(event.price, expected, "price");
}

void verify_order(const order_events::add_order_mpid& event,
                  const fixture& expected) noexcept {
  expect_order_common(event, expected);
  expect_number(event.order_reference_number, expected,
                "order_reference_number");
  expect_ascii(event.buy_sell_indicator, expected, "buy_sell_indicator");
  expect_number(event.shares, expected, "shares");
  expect_ascii(event.stock, expected, "stock");
  expect_number(event.price, expected, "price");
  expect_ascii(event.attribution, expected, "attribution");
}

void verify_order(const order_events::order_executed& event,
                  const fixture& expected) noexcept {
  expect_order_common(event, expected);
  expect_number(event.order_reference_number, expected,
                "order_reference_number");
  expect_number(event.executed_shares, expected, "executed_shares");
  expect_number(event.match_number, expected, "match_number");
}

void verify_order(const order_events::order_executed_with_price& event,
                  const fixture& expected) noexcept {
  expect_order_common(event, expected);
  expect_number(event.order_reference_number, expected,
                "order_reference_number");
  expect_number(event.executed_shares, expected, "executed_shares");
  expect_number(event.match_number, expected, "match_number");
  expect_ascii(event.printable, expected, "printable");
  expect_number(event.execution_price, expected, "execution_price");
}

void verify_order(const order_events::order_cancel& event,
                  const fixture& expected) noexcept {
  expect_order_common(event, expected);
  expect_number(event.order_reference_number, expected,
                "order_reference_number");
  expect_number(event.cancelled_shares, expected, "cancelled_shares");
}

void verify_order(const order_events::order_delete& event,
                  const fixture& expected) noexcept {
  expect_order_common(event, expected);
  expect_number(event.order_reference_number, expected,
                "order_reference_number");
}

void verify_order(const order_events::order_replace& event,
                  const fixture& expected) noexcept {
  expect_order_common(event, expected);
  expect_number(event.original_order_reference_number, expected,
                "original_order_reference_number");
  expect_number(event.new_order_reference_number, expected,
                "new_order_reference_number");
  expect_number(event.shares, expected, "shares");
  expect_number(event.price, expected, "price");
}

void verify_order(const order_events::trade& event,
                  const fixture& expected) noexcept {
  expect_order_common(event, expected);
  expect_number(event.order_reference_number, expected,
                "order_reference_number");
  expect_ascii(event.buy_sell_indicator, expected, "buy_sell_indicator");
  expect_number(event.shares, expected, "shares");
  expect_ascii(event.stock, expected, "stock");
  expect_number(event.price, expected, "price");
  expect_number(event.match_number, expected, "match_number");
}

struct order_sink {
  const fixture* expected{};
  const std::vector<fixture>* fixtures{};
  std::size_t calls{};
  bool stop_after_first{};

  template <class Event>
  feedforge::flow operator()(const Event& event) noexcept {
    ++calls;
    const fixture* selected =
        expected != nullptr
            ? expected
            : find_fixture(*fixtures, Event::source_discriminator);
    if (selected == nullptr) {
      check(false, "order_events sink received unknown event type");
      return feedforge::flow::continue_;
    }
    expect_identity(event, *selected);
    check_field(selected->order_event == Event::event_name, *selected,
                "expected_order_events.event");
    verify_order(event, *selected);
    return stop_after_first ? feedforge::flow::stop
                            : feedforge::flow::continue_;
  }
};

static_assert(order_events::sink_for_all_selected_events<order_sink>);

void check_outcome(const feedforge::decode_outcome outcome,
                   const feedforge::decode_status status,
                   const std::byte message_type,
                   const std::uint16_t expected_size,
                   const std::size_t actual_size,
                   const std::string_view description) {
  check(outcome.status == status, description);
  check(outcome.message_type == message_type, description);
  check(outcome.expected_size == expected_size, description);
  check(outcome.actual_size == actual_size, description);
}

void test_decode(const std::vector<fixture>& fixtures) {
  all_messages::decoder all_decoder;
  order_events::decoder order_decoder;
  std::size_t selected = 0U;
  std::size_t skipped = 0U;
  bool saw_lowercase_h = false;
  bool saw_current_o = false;

  for (const fixture& expected : fixtures) {
    const std::byte type{
        static_cast<unsigned char>(expected.message_type)};
    const auto wire_size =
        static_cast<std::uint16_t>(expected.bytes.size());

    all_sink all_valid{&expected};
    const auto all_outcome = all_decoder.decode_one(expected.bytes, all_valid);
    check_outcome(all_outcome, feedforge::decode_status::emitted, type,
                  wire_size, expected.bytes.size(),
                  "all_messages valid fixture outcome");
    check(all_valid.calls == 1U, "all_messages calls sink exactly once");

    all_sink all_short{&expected};
    const auto short_payload =
        std::span<const std::byte>{expected.bytes}.first(
            expected.bytes.size() - 1U);
    const auto short_outcome =
        all_decoder.decode_one(short_payload, all_short);
    check_outcome(short_outcome,
                  feedforge::decode_status::invalid_message_size, type,
                  wire_size, expected.bytes.size() - 1U,
                  "all_messages size-minus-one");
    check(all_short.calls == 0U, "short payload does not call sink");

    std::vector<std::byte> long_payload = expected.bytes;
    long_payload.push_back(std::byte{0x00});
    all_sink all_long{&expected};
    const auto long_outcome =
        all_decoder.decode_one(long_payload, all_long);
    check_outcome(long_outcome,
                  feedforge::decode_status::invalid_message_size, type,
                  wire_size, expected.bytes.size() + 1U,
                  "all_messages size-plus-one");
    check(all_long.calls == 0U, "long payload does not call sink");

    order_sink projected{&expected};
    const auto projected_outcome =
        order_decoder.decode_one(expected.bytes, projected);
    if (expected.order_result == "emit") {
      ++selected;
      check_outcome(projected_outcome, feedforge::decode_status::emitted,
                    type, wire_size, expected.bytes.size(),
                    "order_events selected fixture");
      check(projected.calls == 1U,
            "order_events selected fixture calls sink once");
    } else {
      ++skipped;
      check_outcome(
          projected_outcome,
          feedforge::decode_status::known_unselected_skipped, type,
          wire_size, expected.bytes.size(),
          "order_events unselected fixture");
      check(projected.calls == 0U,
            "order_events unselected fixture does not call sink");
    }
    saw_lowercase_h =
        saw_lowercase_h || expected.message_type == 'h';
    saw_current_o = saw_current_o || expected.message_type == 'O';
  }

  check(selected == 8U, "order_events emits exactly eight fixtures");
  check(skipped == 15U, "order_events skips exactly fifteen fixtures");
  check(saw_lowercase_h, "lowercase h fixture decoded");
  check(saw_current_o, "current O fixture decoded");

  all_sink unused_all{&fixtures.front()};
  order_sink unused_order{&fixtures.front()};
  const auto empty_all =
      all_decoder.decode_one(std::span<const std::byte>{}, unused_all);
  const auto empty_order =
      order_decoder.decode_one(std::span<const std::byte>{}, unused_order);
  check_outcome(empty_all, feedforge::decode_status::empty_payload,
                std::byte{0x00}, 0U, 0U, "all_messages empty payload");
  check_outcome(empty_order, feedforge::decode_status::empty_payload,
                std::byte{0x00}, 0U, 0U, "order_events empty payload");

  constexpr std::array unknown{std::byte{'?'}};
  const auto unknown_all = all_decoder.decode_one(unknown, unused_all);
  const auto unknown_order =
      order_decoder.decode_one(unknown, unused_order);
  check_outcome(unknown_all,
                feedforge::decode_status::unknown_message_type,
                std::byte{'?'}, 0U, 1U,
                "all_messages unknown payload");
  check_outcome(unknown_order,
                feedforge::decode_status::unknown_message_type,
                std::byte{'?'}, 0U, 1U,
                "order_events unknown payload");

  const fixture* unselected_h =
      find_fixture(fixtures, std::byte{'H'});
  check(unselected_h != nullptr, "uppercase H fixture is present");
  if (unselected_h != nullptr) {
    const auto malformed =
        std::span<const std::byte>{unselected_h->bytes}.first(
            unselected_h->bytes.size() - 1U);
    order_sink sink{unselected_h};
    const auto outcome = order_decoder.decode_one(malformed, sink);
    check_outcome(outcome,
                  feedforge::decode_status::invalid_message_size,
                  std::byte{'H'}, 25U, 24U,
                  "known unselected malformed payload");
    check(sink.calls == 0U,
          "known unselected malformed payload does not call sink");
  }
}

void append_frame(std::vector<std::byte>& file,
                  const std::span<const std::byte> payload) {
  const auto size = static_cast<std::uint16_t>(payload.size());
  file.push_back(std::byte{
      static_cast<unsigned char>((size >> 8U) & 0xffU)});
  file.push_back(
      std::byte{static_cast<unsigned char>(size & 0xffU)});
  file.insert(file.end(), payload.begin(), payload.end());
}

void test_replay(const std::vector<fixture>& fixtures) {
  const fixture* add = find_fixture(fixtures, std::byte{'A'});
  const fixture* system = find_fixture(fixtures, std::byte{'S'});
  const fixture* trading_action =
      find_fixture(fixtures, std::byte{'H'});
  check(add != nullptr && system != nullptr && trading_action != nullptr,
        "replay fixtures are present");
  if (add == nullptr || system == nullptr || trading_action == nullptr) {
    return;
  }

  std::vector<std::byte> every_message;
  for (const fixture& expected : fixtures) {
    append_frame(every_message, expected.bytes);
  }
  every_message.push_back(std::byte{0x00});
  every_message.push_back(std::byte{0x00});

  all_sink all_replay_sink{nullptr, &fixtures};
  const auto all_summary =
      all_messages::replay_binary_file(every_message, all_replay_sink);
  check(all_summary.status == feedforge::replay_status::complete,
        "all_messages replay completes");
  check(all_summary.frames_seen == 23U &&
            all_summary.events_emitted == 23U &&
            all_summary.known_messages_skipped == 0U &&
            all_summary.unknown_messages_skipped == 0U,
        "all_messages replay counters are exact");
  check(all_summary.bytes_consumed == every_message.size(),
        "all_messages replay consumed offset is exact");
  check(all_replay_sink.calls == 23U,
        "all_messages replay verifies every fixture");

  order_sink order_replay_sink{nullptr, &fixtures};
  const auto order_summary =
      order_events::replay_binary_file(every_message, order_replay_sink);
  check(order_summary.status == feedforge::replay_status::complete,
        "order_events replay completes");
  check(order_summary.frames_seen == 23U &&
            order_summary.events_emitted == 8U &&
            order_summary.known_messages_skipped == 15U &&
            order_summary.unknown_messages_skipped == 0U,
        "order_events replay counters are exact");
  check(order_summary.bytes_consumed == every_message.size(),
        "order_events replay consumed offset is exact");
  check(order_replay_sink.calls == 8U,
        "order_events replay emits exactly eight events");

  std::vector<std::byte> complete_two_frames;
  append_frame(complete_two_frames, add->bytes);
  append_frame(complete_two_frames, system->bytes);
  complete_two_frames.push_back(std::byte{0x00});
  complete_two_frames.push_back(std::byte{0x00});
  order_sink complete_sink{add};
  const auto complete = order_events::replay_binary_file(
      complete_two_frames, complete_sink);
  check(complete.status == feedforge::replay_status::complete &&
            complete.frames_seen == 2U &&
            complete.events_emitted == 1U &&
            complete.known_messages_skipped == 1U &&
            complete.bytes_consumed == complete_two_frames.size(),
        "complete multi-frame replay summary is exact");

  std::vector<std::byte> incomplete_two_frames;
  append_frame(incomplete_two_frames, add->bytes);
  append_frame(incomplete_two_frames, system->bytes);
  order_sink incomplete_sink{add};
  const auto incomplete = order_events::replay_binary_file(
      incomplete_two_frames, incomplete_sink);
  check(incomplete.status == feedforge::replay_status::incomplete &&
            incomplete.frames_seen == 2U &&
            incomplete.events_emitted == 1U &&
            incomplete.known_messages_skipped == 1U &&
            incomplete.bytes_consumed == incomplete_two_frames.size(),
        "incomplete multi-frame replay summary is exact");

  constexpr std::array trailing{
      std::byte{0x00}, std::byte{0x00}, std::byte{0xff}};
  order_sink trailing_sink{add};
  const auto trailing_summary =
      order_events::replay_binary_file(trailing, trailing_sink);
  check(trailing_summary.status == feedforge::replay_status::framing_error &&
            trailing_summary.framing_error ==
                feedforge::framing_errc::trailing_data_after_end_marker &&
            trailing_summary.frames_seen == 0U &&
            trailing_summary.bytes_consumed == 2U &&
            trailing_summary.error_offset == 2U,
        "strict trailing-data replay summary is exact");

  const auto malformed_payload =
      std::span<const std::byte>{trading_action->bytes}.first(
          trading_action->bytes.size() - 1U);
  std::vector<std::byte> malformed_known;
  append_frame(malformed_known, malformed_payload);
  order_sink malformed_sink{trading_action};
  const auto malformed_summary =
      order_events::replay_binary_file(malformed_known, malformed_sink);
  check(malformed_summary.status == feedforge::replay_status::decode_error &&
            malformed_summary.frames_seen == 1U &&
            malformed_summary.events_emitted == 0U &&
            malformed_summary.known_messages_skipped == 0U &&
            malformed_summary.bytes_consumed == malformed_known.size() &&
            malformed_summary.error_offset == 2U &&
            malformed_summary.decode_error.status ==
                feedforge::decode_status::invalid_message_size &&
            malformed_summary.decode_error.expected_size == 25U &&
            malformed_summary.decode_error.actual_size == 24U,
        "known-unselected malformed replay summary is exact");

  std::vector<std::byte> unknown_file;
  constexpr std::array unknown_payload{std::byte{'?'}};
  append_frame(unknown_file, unknown_payload);
  unknown_file.push_back(std::byte{0x00});
  unknown_file.push_back(std::byte{0x00});
  order_sink unknown_sink{add};
  const auto unknown_summary =
      order_events::replay_binary_file(unknown_file, unknown_sink);
  check(unknown_summary.status == feedforge::replay_status::decode_error &&
            unknown_summary.frames_seen == 1U &&
            unknown_summary.bytes_consumed == 3U &&
            unknown_summary.error_offset == 2U &&
            unknown_summary.decode_error.status ==
                feedforge::decode_status::unknown_message_type &&
            unknown_summary.decode_error.expected_size == 0U &&
            unknown_summary.decode_error.actual_size == 1U,
        "unknown replay summary and offset are exact");

  std::vector<std::byte> stopped_file;
  append_frame(stopped_file, add->bytes);
  const std::size_t stopping_offset = stopped_file.size();
  stopped_file.push_back(std::byte{0xff});
  order_sink stopped_sink{add, nullptr, 0U, true};
  const auto stopped =
      order_events::replay_binary_file(stopped_file, stopped_sink);
  check(stopped.status == feedforge::replay_status::stopped &&
            stopped.frames_seen == 1U &&
            stopped.events_emitted == 1U &&
            stopped.known_messages_skipped == 0U &&
            stopped.bytes_consumed == stopping_offset &&
            stopped_sink.calls == 1U,
        "cooperative stop summary and consumed offset are exact");
}

}  // namespace

int main() {
  const std::vector<fixture> fixtures = load_fixtures();
  check(fixtures.size() == 23U, "loaded all 23 audited fixtures");
  if (fixtures.size() == 23U) {
    test_decode(fixtures);
    test_replay(fixtures);
  }

  if (failures != 0) {
    std::cerr << failures << " end-to-end integration check(s) failed\n";
    return 1;
  }
  return 0;
}

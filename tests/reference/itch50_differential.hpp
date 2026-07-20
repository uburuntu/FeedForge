#ifndef FEEDFORGE_TESTS_REFERENCE_ITCH50_DIFFERENTIAL_HPP
#define FEEDFORGE_TESTS_REFERENCE_ITCH50_DIFFERENTIAL_HPP

#include "itch50_oracle.hpp"

#include <feedforge/generated/nasdaq/itch50_all.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>

namespace feedforge_reference::itch50 {

namespace generated = feedforge::generated::nasdaq::itch50_all;

[[nodiscard]] constexpr bool all(const std::initializer_list<bool> conditions) noexcept {
  bool result = true;
  for (const bool condition : conditions) {
    result = result && condition;
  }
  return result;
}

class comparing_sink {
public:
  explicit constexpr comparing_sink(const std::span<const std::byte> payload,
                                    const bool stop) noexcept
      : payload_(payload), stop_(stop) {}

  [[nodiscard]] constexpr std::size_t calls() const noexcept { return calls_; }

  [[nodiscard]] constexpr bool values_match() const noexcept { return values_match_; }

  feedforge::flow operator()(const generated::system_event& event) noexcept {
    return finish('S', all({common(event), ascii_equal(event.event_code.raw, 11U)}));
  }

  feedforge::flow operator()(const generated::stock_directory& event) noexcept {
    return finish('R', all({common(event), ascii_equal(event.stock.raw, 11U),
                            ascii_equal(event.market_category.raw, 19U),
                            ascii_equal(event.financial_status_indicator.raw, 20U),
                            integer_equal(event.round_lot_size.value, 21U, 4U),
                            ascii_equal(event.round_lots_only.raw, 25U),
                            ascii_equal(event.issue_classification.raw, 26U),
                            ascii_equal(event.issue_sub_type.raw, 27U),
                            ascii_equal(event.authenticity.raw, 29U),
                            ascii_equal(event.short_sale_threshold_indicator.raw, 30U),
                            ascii_equal(event.ipo_flag.raw, 31U),
                            ascii_equal(event.luld_reference_price_tier.raw, 32U),
                            ascii_equal(event.etp_flag.raw, 33U),
                            integer_equal(event.etp_leverage_factor, 34U, 4U),
                            ascii_equal(event.inverse_indicator.raw, 38U)}));
  }

  feedforge::flow operator()(const generated::stock_trading_action& event) noexcept {
    return finish(
        'H', all({common(event), ascii_equal(event.stock.raw, 11U),
                  ascii_equal(event.trading_state.raw, 19U), ascii_equal(event.reason.raw, 21U)}));
  }

  feedforge::flow operator()(const generated::reg_sho_restriction& event) noexcept {
    return finish('Y', all({common(event), ascii_equal(event.stock.raw, 11U),
                            ascii_equal(event.reg_sho_action.raw, 19U)}));
  }

  feedforge::flow operator()(const generated::market_participant_position& event) noexcept {
    return finish('L', all({common(event), ascii_equal(event.mpid.raw, 11U),
                            ascii_equal(event.stock.raw, 15U),
                            ascii_equal(event.primary_market_maker.raw, 23U),
                            ascii_equal(event.market_maker_mode.raw, 24U),
                            ascii_equal(event.market_participant_state.raw, 25U)}));
  }

  feedforge::flow operator()(const generated::mwcb_decline_level& event) noexcept {
    return finish('V', all({common(event), integer_equal(event.level_1.raw, 11U, 8U),
                            integer_equal(event.level_2.raw, 19U, 8U),
                            integer_equal(event.level_3.raw, 27U, 8U)}));
  }

  feedforge::flow operator()(const generated::mwcb_status& event) noexcept {
    return finish('W', all({common(event), ascii_equal(event.breached_level.raw, 11U)}));
  }

  feedforge::flow operator()(const generated::ipo_quoting_period_update& event) noexcept {
    return finish('K', all({common(event), ascii_equal(event.stock.raw, 11U),
                            integer_equal(event.ipo_quotation_release_time, 19U, 4U),
                            ascii_equal(event.ipo_quotation_release_qualifier.raw, 23U),
                            integer_equal(event.ipo_price.raw, 24U, 4U)}));
  }

  feedforge::flow operator()(const generated::luld_auction_collar& event) noexcept {
    return finish('J', all({common(event), ascii_equal(event.stock.raw, 11U),
                            integer_equal(event.auction_collar_reference_price.raw, 19U, 4U),
                            integer_equal(event.upper_auction_collar_price.raw, 23U, 4U),
                            integer_equal(event.lower_auction_collar_price.raw, 27U, 4U),
                            integer_equal(event.auction_collar_extension, 31U, 4U)}));
  }

  feedforge::flow operator()(const generated::operational_halt& event) noexcept {
    return finish('h', all({common(event), ascii_equal(event.stock.raw, 11U),
                            ascii_equal(event.market_code.raw, 19U),
                            ascii_equal(event.operational_halt_action.raw, 20U)}));
  }

  feedforge::flow operator()(const generated::add_order& event) noexcept {
    return finish(
        'A', all({common(event), integer_equal(event.order_reference_number.value, 11U, 8U),
                  ascii_equal(event.buy_sell_indicator.raw, 19U),
                  integer_equal(event.shares.value, 20U, 4U), ascii_equal(event.stock.raw, 24U),
                  integer_equal(event.price.raw, 32U, 4U)}));
  }

  feedforge::flow operator()(const generated::add_order_mpid& event) noexcept {
    return finish(
        'F',
        all({common(event), integer_equal(event.order_reference_number.value, 11U, 8U),
             ascii_equal(event.buy_sell_indicator.raw, 19U),
             integer_equal(event.shares.value, 20U, 4U), ascii_equal(event.stock.raw, 24U),
             integer_equal(event.price.raw, 32U, 4U), ascii_equal(event.attribution.raw, 36U)}));
  }

  feedforge::flow operator()(const generated::order_executed& event) noexcept {
    return finish('E',
                  all({common(event), integer_equal(event.order_reference_number.value, 11U, 8U),
                       integer_equal(event.executed_shares.value, 19U, 4U),
                       integer_equal(event.match_number.value, 23U, 8U)}));
  }

  feedforge::flow operator()(const generated::order_executed_with_price& event) noexcept {
    return finish('C',
                  all({common(event), integer_equal(event.order_reference_number.value, 11U, 8U),
                       integer_equal(event.executed_shares.value, 19U, 4U),
                       integer_equal(event.match_number.value, 23U, 8U),
                       ascii_equal(event.printable.raw, 31U),
                       integer_equal(event.execution_price.raw, 32U, 4U)}));
  }

  feedforge::flow operator()(const generated::order_cancel& event) noexcept {
    return finish('X',
                  all({common(event), integer_equal(event.order_reference_number.value, 11U, 8U),
                       integer_equal(event.cancelled_shares.value, 19U, 4U)}));
  }

  feedforge::flow operator()(const generated::order_delete& event) noexcept {
    return finish('D',
                  all({common(event), integer_equal(event.order_reference_number.value, 11U, 8U)}));
  }

  feedforge::flow operator()(const generated::order_replace& event) noexcept {
    return finish(
        'U',
        all({common(event), integer_equal(event.original_order_reference_number.value, 11U, 8U),
             integer_equal(event.new_order_reference_number.value, 19U, 8U),
             integer_equal(event.shares.value, 27U, 4U), integer_equal(event.price.raw, 31U, 4U)}));
  }

  feedforge::flow operator()(const generated::trade& event) noexcept {
    return finish('P',
                  all({common(event), integer_equal(event.order_reference_number.value, 11U, 8U),
                       ascii_equal(event.buy_sell_indicator.raw, 19U),
                       integer_equal(event.shares.value, 20U, 4U),
                       ascii_equal(event.stock.raw, 24U), integer_equal(event.price.raw, 32U, 4U),
                       integer_equal(event.match_number.value, 36U, 8U)}));
  }

  feedforge::flow operator()(const generated::cross_trade& event) noexcept {
    return finish(
        'Q', all({common(event), integer_equal(event.shares, 11U, 8U),
                  ascii_equal(event.stock.raw, 19U), integer_equal(event.cross_price.raw, 27U, 4U),
                  integer_equal(event.match_number.value, 31U, 8U),
                  ascii_equal(event.cross_type.raw, 39U)}));
  }

  feedforge::flow operator()(const generated::broken_trade& event) noexcept {
    return finish('B', all({common(event), integer_equal(event.match_number.value, 11U, 8U)}));
  }

  feedforge::flow operator()(const generated::net_order_imbalance_indicator& event) noexcept {
    return finish(
        'I', all({common(event), integer_equal(event.paired_shares, 11U, 8U),
                  integer_equal(event.imbalance_shares, 19U, 8U),
                  ascii_equal(event.imbalance_direction.raw, 27U),
                  ascii_equal(event.stock.raw, 28U), integer_equal(event.far_price.raw, 36U, 4U),
                  integer_equal(event.near_price.raw, 40U, 4U),
                  integer_equal(event.current_reference_price.raw, 44U, 4U),
                  ascii_equal(event.cross_type.raw, 48U),
                  ascii_equal(event.price_variation_indicator.raw, 49U)}));
  }

  feedforge::flow operator()(const generated::retail_price_improvement_indicator& event) noexcept {
    return finish('N', all({common(event), ascii_equal(event.stock.raw, 11U),
                            ascii_equal(event.interest_flag.raw, 19U)}));
  }

  feedforge::flow operator()(const generated::direct_listing_with_capital_raise& event) noexcept {
    return finish('O', all({common(event), ascii_equal(event.stock.raw, 11U),
                            ascii_equal(event.open_eligibility_status.raw, 19U),
                            integer_equal(event.minimum_allowable_price.raw, 20U, 4U),
                            integer_equal(event.maximum_allowable_price.raw, 24U, 4U),
                            integer_equal(event.near_execution_price.raw, 28U, 4U),
                            integer_equal(event.near_execution_time, 32U, 8U),
                            integer_equal(event.lower_price_range_collar.raw, 40U, 4U),
                            integer_equal(event.upper_price_range_collar.raw, 44U, 4U)}));
  }

private:
  template <class Event> [[nodiscard]] bool common(const Event& event) const noexcept {
    return all({integer_equal(event.stock_locate.value, 1U, 2U),
                integer_equal(event.tracking_number.value, 3U, 2U),
                integer_equal(event.timestamp.value, 5U, 6U)});
  }

  template <class Integer>
  [[nodiscard]] bool integer_equal(const Integer actual, const std::size_t offset,
                                   const std::size_t width) const noexcept {
    const unsigned_read expected = read_unsigned(payload_, offset, width);
    return expected.valid && expected.value == static_cast<std::uint64_t>(actual);
  }

  template <std::size_t Size>
  [[nodiscard]] bool ascii_equal(const std::array<char, Size>& actual,
                                 const std::size_t offset) const noexcept {
    return bytes_equal(payload_, offset, std::span<const char>{actual.data(), actual.size()});
  }

  feedforge::flow finish(const char discriminator, const bool fields_match) noexcept {
    ++calls_;
    const bool discriminator_matches =
        !payload_.empty() && payload_.front() == octet(discriminator);
    values_match_ = values_match_ && discriminator_matches && fields_match;
    return stop_ ? feedforge::flow::stop : feedforge::flow::continue_;
  }

  std::span<const std::byte> payload_;
  std::size_t calls_{};
  bool values_match_{true};
  bool stop_{};
};

static_assert(generated::sink_for_all_selected_events<comparing_sink>);

struct differential_observation {
  decode_outcome expected{};
  feedforge::decode_outcome actual{};
  std::size_t sink_calls{};
  bool event_values_match{};
  bool stop_requested{};

  [[nodiscard]] constexpr bool matches() const noexcept;
};

[[nodiscard]] constexpr feedforge::decode_status
expected_generated_status(const decode_status status, const bool stop_requested) noexcept {
  switch (status) {
  case decode_status::emitted:
    return stop_requested ? feedforge::decode_status::stopped : feedforge::decode_status::emitted;
  case decode_status::empty_payload:
    return feedforge::decode_status::empty_payload;
  case decode_status::unknown_message_type:
    return feedforge::decode_status::unknown_message_type;
  case decode_status::invalid_message_size:
    return feedforge::decode_status::invalid_message_size;
  }
  return feedforge::decode_status::empty_payload;
}

[[nodiscard]] constexpr bool differential_observation::matches() const noexcept {
  const bool expects_event = expected.status == decode_status::emitted;
  return actual.status == expected_generated_status(expected.status, stop_requested) &&
         actual.message_type == expected.message_type &&
         actual.expected_size == expected.expected_size &&
         actual.actual_size == expected.actual_size && sink_calls == (expects_event ? 1U : 0U) &&
         (!expects_event || event_values_match);
}

[[nodiscard]] inline differential_observation compare_once(const std::span<const std::byte> payload,
                                                           const bool request_stop) noexcept {
  comparing_sink sink{payload, request_stop};
  generated::decoder decoder;
  const feedforge::decode_outcome actual = decoder.decode_one(payload, sink);
  return {classify(payload), actual, sink.calls(), sink.values_match(), request_stop};
}

// Primary callable harness for unit tests and fuzz targets. Both sink flow
// branches are checked for every payload.
[[nodiscard]] inline bool matches_generated(const std::span<const std::byte> payload) noexcept {
  return compare_once(payload, false).matches() && compare_once(payload, true).matches();
}

} // namespace feedforge_reference::itch50

#endif // FEEDFORGE_TESTS_REFERENCE_ITCH50_DIFFERENTIAL_HPP

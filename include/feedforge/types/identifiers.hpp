#ifndef FEEDFORGE_TYPES_IDENTIFIERS_HPP
#define FEEDFORGE_TYPES_IDENTIFIERS_HPP

#include <cstdint>
#include <type_traits>

#include <feedforge/config.hpp>

namespace feedforge {

template <class Tag, class Rep>
struct integer_value {
  static_assert(std::is_integral_v<Rep> && std::is_unsigned_v<Rep>,
                "FeedForge integer representations must be unsigned integers");

  Rep value{};

  friend constexpr bool operator==(integer_value, integer_value) = default;
};

struct stock_locate_tag {};
struct tracking_number_tag {};
struct order_reference_number_tag {};
struct match_number_tag {};
struct share_count_tag {};

using stock_locate = integer_value<stock_locate_tag, std::uint16_t>;
using tracking_number = integer_value<tracking_number_tag, std::uint16_t>;
using order_reference_number =
    integer_value<order_reference_number_tag, std::uint64_t>;
using match_number = integer_value<match_number_tag, std::uint64_t>;
using share_count = integer_value<share_count_tag, std::uint32_t>;

}  // namespace feedforge

#endif  // FEEDFORGE_TYPES_IDENTIFIERS_HPP

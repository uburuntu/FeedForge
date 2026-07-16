#ifndef FEEDFORGE_TYPES_DECIMAL_HPP
#define FEEDFORGE_TYPES_DECIMAL_HPP

#include <type_traits>

#include <feedforge/config.hpp>

namespace feedforge {

template <class Rep, int Scale>
struct decimal {
  static_assert(std::is_integral_v<Rep> && std::is_unsigned_v<Rep>,
                "FeedForge decimal representations must be unsigned integers");
  static_assert(Scale >= 0 && Scale <= 18,
                "FeedForge decimal scales must be in the range [0, 18]");

  Rep raw{};
  static constexpr int scale = Scale;

  friend constexpr bool operator==(decimal, decimal) = default;
};

}  // namespace feedforge

#endif  // FEEDFORGE_TYPES_DECIMAL_HPP

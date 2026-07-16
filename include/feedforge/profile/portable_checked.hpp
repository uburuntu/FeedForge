#ifndef FEEDFORGE_PROFILE_PORTABLE_CHECKED_HPP
#define FEEDFORGE_PROFILE_PORTABLE_CHECKED_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <feedforge/wire/load_big_endian.hpp>

namespace feedforge::profile {

struct portable_checked {
  static constexpr std::string_view variant_id = "portable_checked.v1";

  template <std::size_t Width>
    requires wire::supported_unsigned_width<Width>
  [[nodiscard]] static constexpr std::uint64_t
  load_unsigned(std::byte const* first) noexcept {
    return wire::load_unsigned<Width>(first);
  }
};

}  // namespace feedforge::profile

#endif  // FEEDFORGE_PROFILE_PORTABLE_CHECKED_HPP

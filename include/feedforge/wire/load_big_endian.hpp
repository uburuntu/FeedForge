#ifndef FEEDFORGE_WIRE_LOAD_BIG_ENDIAN_HPP
#define FEEDFORGE_WIRE_LOAD_BIG_ENDIAN_HPP

#include <cstddef>
#include <cstdint>

#include <feedforge/config.hpp>

namespace feedforge::wire {

template <std::size_t Width>
inline constexpr bool supported_unsigned_width =
    Width == 1 || Width == 2 || Width == 4 || Width == 6 || Width == 8;

template <std::size_t Width>
  requires supported_unsigned_width<Width>
[[nodiscard]] constexpr std::uint64_t
load_unsigned(std::byte const* first) noexcept {
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < Width; ++index) {
    value = (value << 8U) |
            static_cast<std::uint64_t>(std::to_integer<unsigned int>(first[index]));
  }
  return value;
}

}  // namespace feedforge::wire

#endif  // FEEDFORGE_WIRE_LOAD_BIG_ENDIAN_HPP

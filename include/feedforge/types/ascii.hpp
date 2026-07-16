#ifndef FEEDFORGE_TYPES_ASCII_HPP
#define FEEDFORGE_TYPES_ASCII_HPP

#include <array>
#include <cstddef>
#include <string_view>

#include <feedforge/config.hpp>

namespace feedforge {

template <std::size_t N>
struct ascii {
  std::array<char, N> raw{};

  [[nodiscard]] constexpr std::string_view trimmed() const noexcept {
    std::size_t length = N;
    while (length != 0 && raw[length - 1] == ' ') {
      --length;
    }
    return std::string_view{raw.data(), length};
  }

  friend constexpr bool operator==(ascii, ascii) = default;
};

}  // namespace feedforge

#endif  // FEEDFORGE_TYPES_ASCII_HPP

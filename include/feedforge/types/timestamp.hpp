#ifndef FEEDFORGE_TYPES_TIMESTAMP_HPP
#define FEEDFORGE_TYPES_TIMESTAMP_HPP

#include <cstdint>

#include <feedforge/config.hpp>

namespace feedforge {

struct timestamp_ns {
  std::uint64_t value{};

  friend constexpr bool operator==(timestamp_ns, timestamp_ns) = default;
};

}  // namespace feedforge

#endif  // FEEDFORGE_TYPES_TIMESTAMP_HPP

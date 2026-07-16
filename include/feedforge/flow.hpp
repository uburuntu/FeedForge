#ifndef FEEDFORGE_FLOW_HPP
#define FEEDFORGE_FLOW_HPP

#include <cstdint>

#include <feedforge/config.hpp>

namespace feedforge {

enum class flow : std::uint8_t {
  continue_,
  stop,
};

}  // namespace feedforge

#endif  // FEEDFORGE_FLOW_HPP

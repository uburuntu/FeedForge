#ifndef FEEDFORGE_VERSION_HPP
#define FEEDFORGE_VERSION_HPP

#include <cstdint>
#include <string_view>

#include <feedforge/config.hpp>

namespace feedforge {

inline constexpr std::uint32_t runtime_api_epoch = 1;
inline constexpr std::uint32_t runtime_api_revision = 0;
inline constexpr std::string_view version_string = "0.3.0";

}  // namespace feedforge

#endif  // FEEDFORGE_VERSION_HPP

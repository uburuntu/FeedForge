#ifndef FEEDFORGE_VERSION_HPP
#define FEEDFORGE_VERSION_HPP

#include <cstdint>
#include <string_view>

#include <feedforge/config.hpp>

namespace feedforge {

inline constexpr std::uint32_t runtime_api_version = 2;
inline constexpr std::string_view version_string = "0.2.0";

}  // namespace feedforge

#endif  // FEEDFORGE_VERSION_HPP

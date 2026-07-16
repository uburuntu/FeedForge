#ifndef FEEDFORGE_PROFILE_CONCEPTS_HPP
#define FEEDFORGE_PROFILE_CONCEPTS_HPP

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <feedforge/flow.hpp>

namespace feedforge {

template <class T>
concept decoder_implementation = requires(std::byte const* first) {
  { T::variant_id } -> std::convertible_to<std::string_view>;
  { T::template load_unsigned<1>(first) } noexcept
      -> std::same_as<std::uint64_t>;
  { T::template load_unsigned<2>(first) } noexcept
      -> std::same_as<std::uint64_t>;
  { T::template load_unsigned<4>(first) } noexcept
      -> std::same_as<std::uint64_t>;
  { T::template load_unsigned<6>(first) } noexcept
      -> std::same_as<std::uint64_t>;
  { T::template load_unsigned<8>(first) } noexcept
      -> std::same_as<std::uint64_t>;
};

template <class Sink, class Event>
concept sink_for = requires(Sink& sink, Event const& event) {
  { sink(event) } noexcept -> std::same_as<feedforge::flow>;
};

}  // namespace feedforge

#endif  // FEEDFORGE_PROFILE_CONCEPTS_HPP
